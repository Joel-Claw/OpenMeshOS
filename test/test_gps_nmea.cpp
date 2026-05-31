// OpenMeshOS — GPS NMEA parsing unit tests (host-side)
// Copyright 2026 Joel Claw & contributors — WTFPL v2
//
// Validates NMEA sentence parsing, coordinate extraction,
// and integration with MapEngine coordinate conversion.
// This runs on host (x86/ARM) without Arduino or GPS hardware.
//
// We implement a minimal NMEA parser to validate our GPS data
// handling assumptions, and test that real-world NMEA sentences
// produce correct lat/lng values that MapEngine can convert
// to tile coordinates.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// ── Minimal NMEA sentence parser (for testing) ────────────────────
// Parses GPGGA, GPRMC, and GPGSA sentences to extract:
//   - Latitude, Longitude (degrees)
//   - Altitude (meters)
//   - Speed (knots)
//   - Course (degrees)
//   - Satellite count
//   - Fix quality
//   - HDOP

struct GpsFix {
    double  lat = 0.0;         // decimal degrees, positive = N
    double  lng = 0.0;         // decimal degrees, positive = E
    double  altitude = 0.0;    // meters above MSL
    double  speed = 0.0;       // knots
    double  course = 0.0;      // degrees true
    int     satellites = 0;
    int     fixQuality = 0;    // 0=none, 1=GPS, 2=DGPS
    double  hdop = 0.0;
    bool    valid = false;
};

/// Parse NMEA lat/lng from raw field + hemisphere.
/// "4936.6920" + "N" = 49.61153 degrees
/// "00611.2574" + "E" = 6.18762 degrees
static double parseNmeaCoord(const char* raw, char hemisphere) {
    // Find the decimal point
    const char* dot = strchr(raw, '.');
    if (!dot) return 0.0;

    // Degrees: all digits before the dot minus the last 2 (minutes)
    size_t dotPos = dot - raw;
    if (dotPos < 3) return 0.0;  // need at least dmm.m

    // Minutes: last 2 digits before dot + fractional minutes
    char degStr[16] = {};
    strncpy(degStr, raw, dotPos - 2);
    degStr[dotPos - 2] = '\0';

    double degrees = atof(degStr);
    double minutes = atof(raw + dotPos - 2);  // includes integer + fractional

    double result = degrees + minutes / 60.0;

    if (hemisphere == 'S' || hemisphere == 'W') result = -result;
    return result;
}

/// Parse a single NMEA sentence and fill GpsFix.
/// Supports GPGGA and GPRMC.
static bool parseNmea(const char* sentence, GpsFix& fix) {
    if (sentence[0] != '$') return false;

    // Make a mutable copy
    char buf[256];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    // Verify checksum
    char* asterisk = strchr(buf, '*');
    if (asterisk) {
        uint8_t calc = 0;
        for (const char* p = buf + 1; p < asterisk; p++) {
            calc ^= (uint8_t)*p;
        }
        int given = 0;
        sscanf(asterisk + 1, "%02X", &given);
        if (calc != given) return false;
        *asterisk = '\0';  // truncate for field splitting
    }

    // Split into fields (handle consecutive commas = empty fields)
    char fieldStorage[32][64] = {};
    const char* fields[32] = {};
    int nFields = 0;
    const char* start = buf;
    while (nFields < 32) {
        const char* comma = strchr(start, ',');
        if (comma) {
            size_t len = comma - start;
            if (len >= sizeof(fieldStorage[0])) len = sizeof(fieldStorage[0]) - 1;
            memcpy(fieldStorage[nFields], start, len);
            fieldStorage[nFields][len] = '\0';
            fields[nFields] = fieldStorage[nFields];
            start = comma + 1;
        } else {
            // Last field
            size_t len = strlen(start);
            if (len >= sizeof(fieldStorage[0])) len = sizeof(fieldStorage[0]) - 1;
            memcpy(fieldStorage[nFields], start, len);
            fieldStorage[nFields][len] = '\0';
            fields[nFields] = fieldStorage[nFields];
            nFields++;
            break;
        }
        nFields++;
    }

    if (nFields < 2) return false;

    const char* type = fields[0];

    // ── GPGGA: Global Positioning System Fix Data ─────────────────
    // $GPGGA,time,lat,N/S,lng,E/W,quality,numSV,HDOP,alt,M,sep,M,diffAge,diffStation*cs
    if (strcmp(type, "$GPGGA") == 0 && nFields >= 10) {
        if (fields[2] && fields[2][0] != '\0' && fields[4] && fields[4][0] != '\0') {
            fix.lat = parseNmeaCoord(fields[2], fields[3][0]);
            fix.lng = parseNmeaCoord(fields[4], fields[5][0]);
        }
        fix.fixQuality = (fields[6] && fields[6][0]) ? atoi(fields[6]) : 0;
        fix.satellites = (fields[7] && fields[7][0]) ? atoi(fields[7]) : 0;
        fix.hdop = (fields[8] && fields[8][0]) ? atof(fields[8]) : 0.0;
        if (fields[9] && fields[9][0]) fix.altitude = atof(fields[9]);
        if (fix.fixQuality > 0) fix.valid = true;
        return true;
    }

    // ── GPRMC: Recommended Minimum Specific GNSS Data ──────────────
    // $GPRMC,time,status,lat,N/S,lng,E/W,speed,course,date,magVar,magVarDir*cs
    if (strcmp(type, "$GPRMC") == 0 && nFields >= 9) {
        if (nFields > 2 && fields[2] && fields[2][0] != 'A') return true;  // status V = no fix
        if (fields[3] && fields[3][0] != '\0' && fields[5] && fields[5][0] != '\0') {
            fix.lat = parseNmeaCoord(fields[3], fields[4][0]);
            fix.lng = parseNmeaCoord(fields[5], fields[6][0]);
        }
        if (fields[7] && fields[7][0]) fix.speed = atof(fields[7]);   // knots
        if (fields[8] && fields[8][0]) fix.course = atof(fields[8]);  // degrees
        fix.valid = true;
        return true;
    }

    return false;
}

// ── MapEngine coordinate conversion (duplicated for host test) ────
static void latLngToTile(double lat, double lng, int z, int& tx, int& ty) {
    double n = pow(2.0, z);
    tx = (int)((lng + 180.0) / 360.0 * n);
    double latRad = lat * M_PI / 180.0;
    ty = (int)((1.0 - log(tan(latRad) + 1.0 / cos(latRad)) / M_PI) / 2.0 * n);
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    int maxTile = (1 << z) - 1;
    if (tx > maxTile) tx = maxTile;
    if (ty > maxTile) ty = maxTile;
}

// ── Test framework ─────────────────────────────────────────────────
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT_TRUE(expr, msg) do { \
    tests_run++; \
    if (expr) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s\n", msg); } \
} while(0)

#define ASSERT_EQ(a, b, msg) do { \
    tests_run++; \
    if ((a) == (b)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s — expected %d, got %d\n", msg, (int)(b), (int)(a)); } \
} while(0)

#define ASSERT_NEAR(a, b, eps, msg) do { \
    tests_run++; \
    if (fabs((a) - (b)) < (eps)) { tests_passed++; } \
    else { tests_failed++; printf("  FAIL: %s — expected %.8f, got %.8f (eps %.8f)\n", msg, (double)(b), (double)(a), (double)(eps)); } \
} while(0)

// ── Test cases ─────────────────────────────────────────────────────

void test_nmea_coord_parsing_ddmm() {
    // Standard DDMM.MMMM format
    double lat = parseNmeaCoord("4936.6920", 'N');
    ASSERT_NEAR(lat, 49.61153, 0.0001, "NMEA lat 4936.6920N = 49.61153");

    double lng = parseNmeaCoord("00611.2574", 'E');
    ASSERT_NEAR(lng, 6.18762, 0.0001, "NMEA lng 00611.2574E = 6.18762");
}

void test_nmea_coord_south_west() {
    double lat = parseNmeaCoord("3354.4234", 'S');
    ASSERT_NEAR(lat, -33.90706, 0.0001, "NMEA lat 3354.4234S = -33.90706");

    double lng = parseNmeaCoord("15112.5104", 'W');
    ASSERT_NEAR(lng, -151.20851, 0.0001, "NMEA lng 15112.5104W = -151.20851");
}

void test_nmea_coord_equator_prime_meridian() {
    double lat = parseNmeaCoord("0000.0000", 'N');
    ASSERT_NEAR(lat, 0.0, 0.0001, "NMEA lat 0000.0000N = 0");

    double lng = parseNmeaCoord("00000.0000", 'E');
    ASSERT_NEAR(lng, 0.0, 0.0001, "NMEA lng 00000.0000E = 0");
}

void test_nmea_gga_parsing() {
    // Real GPGGA sentence (Luxembourg area)
    const char* gga = "$GPGGA,123519,4936.692,N,00611.257,E,1,08,0.9,228.2,M,46.9,M,,*4C";

    GpsFix fix = {};
    bool ok = parseNmea(gga, fix);

    ASSERT_TRUE(ok, "GGA parse succeeds");
    ASSERT_TRUE(fix.valid, "GGA fix is valid");
    ASSERT_NEAR(fix.lat, 49.61153, 0.0001, "GGA lat = 49.61153");
    ASSERT_NEAR(fix.lng, 6.18762, 0.0001, "GGA lng = 6.18762");
    ASSERT_EQ(fix.fixQuality, 1, "GGA fix quality = 1 (GPS)");
    ASSERT_EQ(fix.satellites, 8, "GGA satellites = 8");
    ASSERT_NEAR(fix.hdop, 0.9, 0.01, "GGA HDOP = 0.9");
    ASSERT_NEAR(fix.altitude, 228.2, 0.01, "GGA altitude = 228.2m");
}

void test_nmea_rmc_parsing() {
    // Real GPRMC sentence (Sydney area) — note: trailing comma fields may be empty
    const char* rmc = "$GPRMC,123519,A,3354.423,S,15112.510,E,0.0,0.0,050125,,*08";

    GpsFix fix = {};
    bool ok = parseNmea(rmc, fix);

    ASSERT_TRUE(ok, "RMC parse succeeds");
    ASSERT_TRUE(fix.valid, "RMC fix is valid");
    ASSERT_NEAR(fix.lat, -33.90708, 0.0001, "RMC lat = -33.90708");
    ASSERT_NEAR(fix.lng, 151.20850, 0.0001, "RMC lng = 151.20850");
    ASSERT_NEAR(fix.speed, 0.0, 0.01, "RMC speed = 0.0 knots");
    ASSERT_NEAR(fix.course, 0.0, 0.01, "RMC course = 0.0 degrees");
}

void test_nmea_rmc_no_fix() {
    const char* rmc = "$GPRMC,123519,V,,,,,,,050125,,*3F";

    GpsFix fix = {};
    bool ok = parseNmea(rmc, fix);

    ASSERT_TRUE(ok, "RMC V parse succeeds");
    ASSERT_TRUE(!fix.valid, "RMC V fix is NOT valid");
}

void test_nmea_checksum_valid() {
    // Correctly checksummed GGA
    const char* gga = "$GPGGA,123519,4936.692,N,00611.257,E,1,08,0.9,228.2,M,46.9,M,,*4C";
    GpsFix fix = {};
    bool ok = parseNmea(gga, fix);
    ASSERT_TRUE(ok, "Valid checksum GGA parses");
}

void test_nmea_checksum_invalid() {
    // Corrupted checksum (should be 4C, not 42)
    const char* gga = "$GPGGA,123519,4936.692,N,00611.257,E,1,08,0.9,228.2,M,46.9,M,,*42";
    GpsFix fix = {};
    bool ok = parseNmea(gga, fix);
    ASSERT_TRUE(!ok, "Invalid checksum GGA rejected");
}

void test_nmea_to_tile_luxembourg() {
    // Luxembourg: 49.61153 N, 6.18762 E → zoom 10
    GpsFix fix;
    fix.lat = 49.61153;
    fix.lng = 6.18762;
    fix.valid = true;

    int tx, ty;
    latLngToTile(fix.lat, fix.lng, 10, tx, ty);
    ASSERT_EQ(tx, 529, "Lux z10 tile X = 529");
    ASSERT_EQ(ty, 348, "Lux z10 tile Y = 348");
}

void test_nmea_to_tile_sydney() {
    // Sydney: -33.90705 S, 151.20850 E → z10 tile
    GpsFix fix;
    fix.lat = -33.90705;
    fix.lng = 151.20850;
    fix.valid = true;

    int tx, ty;
    latLngToTile(fix.lat, fix.lng, 10, tx, ty);
    ASSERT_EQ(tx, 942, "Sydney z10 tile X = 942");
    ASSERT_EQ(ty, 614, "Sydney z10 tile Y = 614");
}

void test_nmea_to_tile_new_york() {
    // NYC: 40.7128 N, -74.0060 W → z10 tile
    GpsFix fix;
    fix.lat = 40.7128;
    fix.lng = -74.0060;
    fix.valid = true;

    int tx, ty;
    latLngToTile(fix.lat, fix.lng, 10, tx, ty);
    ASSERT_EQ(tx, 301, "NYC z10 tile X = 301");
    ASSERT_EQ(ty, 385, "NYC z10 tile Y = 385");
}

void test_nmea_to_tile_null_island() {
    // 0,0 → z0 tile (single tile)
    GpsFix fix;
    fix.lat = 0.0;
    fix.lng = 0.0;
    fix.valid = true;

    int tx, ty;
    latLngToTile(fix.lat, fix.lng, 0, tx, ty);
    ASSERT_EQ(tx, 0, "Null Island z0 tile X = 0");
    ASSERT_EQ(ty, 0, "Null Island z0 tile Y = 0");
}

void test_nmea_round_trip_coords() {
    // Parse NMEA → decimal → tile, check consistency
    const char* gga = "$GPGGA,092750.00,4936.6920,N,00611.2574,E,1,05,1.4,228.2,M,46.9,M,,*63";

    GpsFix fix = {};
    bool ok = parseNmea(gga, fix);

    // Verify checksum first (we calculated it)
    ASSERT_TRUE(ok, "GGA round-trip parse succeeds");
    ASSERT_TRUE(fix.valid, "GGA round-trip fix is valid");

    // lat should be ~49.61153
    ASSERT_NEAR(fix.lat, 49.61153, 0.0002, "GGA round-trip lat");
    ASSERT_NEAR(fix.lng, 6.18762, 0.0002, "GGA round-trip lng");

    // Now convert to tile at z=15 (typical detail level)
    int tx, ty;
    latLngToTile(fix.lat, fix.lng, 15, tx, ty);

    // z=15, lat=49.61, lng=6.19 → tile should be x=16947, y=11167
    ASSERT_EQ(tx, 16947, "GGA round-trip z15 tile X");
    ASSERT_EQ(ty, 11167, "GGA round-trip z15 tile Y");
}

void test_nmea_speed_knots_to_kmh() {
    // 1 knot = 1.852 km/h
    double speedKnots = 25.0;
    double speedKmh = speedKnots * 1.852;
    ASSERT_NEAR(speedKmh, 46.3, 0.1, "25 knots = 46.3 km/h");
}

void test_nmea_empty_fields() {
    // GGA with empty lat/lng (no fix yet)
    const char* gga = "$GPGGA,092750.00,,,,,0,00,,,,,,,*41";
    GpsFix fix = {};
    parseNmea(gga, fix);
    // Should parse but lat/lng stay 0
    ASSERT_TRUE(!fix.valid, "Empty GGA fields = no valid fix");
    ASSERT_EQ(fix.fixQuality, 0, "Empty GGA fix quality = 0");
    ASSERT_EQ(fix.satellites, 0, "Empty GGA satellites = 0");
}

// ── Main ───────────────────────────────────────────────────────────
int main() {
    printf("OpenMeshOS GPS NMEA Tests\n");
    printf("==========================\n\n");

    printf("NMEA coordinate parsing:\n");
    test_nmea_coord_parsing_ddmm();
    test_nmea_coord_south_west();
    test_nmea_coord_equator_prime_meridian();

    printf("\nGGA sentence parsing:\n");
    test_nmea_gga_parsing();
    test_nmea_checksum_valid();
    test_nmea_checksum_invalid();
    test_nmea_empty_fields();

    printf("\nRMC sentence parsing:\n");
    test_nmea_rmc_parsing();
    test_nmea_rmc_no_fix();

    printf("\nGPS → tile coordinate mapping:\n");
    test_nmea_to_tile_luxembourg();
    test_nmea_to_tile_sydney();
    test_nmea_to_tile_new_york();
    test_nmea_to_tile_null_island();
    test_nmea_round_trip_coords();

    printf("\nUnit conversions:\n");
    test_nmea_speed_knots_to_kmh();

    printf("\n==========================\n");
    printf("Results: %d/%d passed, %d failed\n", tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}