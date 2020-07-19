/**
 * This is an example program that demonstrates how to find seeds with a
 * quad-witch-hut located around the specified region (512x512 area).
 *
 * It uses some optimisations that cause it miss a small number of seeds, in
 * exchange for a major speed upgrade. (~99% accuracy, ~1200% speed)
 */

#include "finders.h"

#include <unistd.h>
#include <errno.h>
#include <limits.h>

const char *versions[] = {"1.7", "1.8", "1.9", "1.10", "1.11", "1.12", "1.13", "1.13.2", "1.14", "1.15", "UNKNOWN"};

static unsigned int str2int(const char *str, int h) {
    return !str[h] ? 5381 : (str2int(str, h + 1) * 33) ^ (unsigned int) (str[h]);
}

enum versions parse_version(char *s) {
    enum versions v;
    unsigned int value = str2int(s, 0);
    switch (value) {
        case 193357645 :
            v = MC_1_7;
            break;
        case 193366850:
            v = MC_1_8;
            break;
        case 193367875 :
            v = MC_1_9;
            break;
        case 2085846491 :
            v = MC_1_10;
            break;
        case 2085882490 :
            v = MC_1_11;
            break;
        case 2085918233 :
            v = MC_1_12;
            break;
        case 2085954232 :
            v = MC_1_13;
            break;
        case 3841915620:
            v = MC_1_13_2;
            break;
        case 2085703007:
            v = MC_1_14;
            break;
        case 2085739006:
            v = MC_1_15;
            break;
        default:
            v = MC_LEG;
            break;
    }
    return v;
}

char *inputString(FILE *fp, size_t size) {
    //The size is extended by the input with the value of the provisional
    char *str;
    int ch;
    size_t len = 0;
    str = realloc(NULL, sizeof(char) * size);//size is start size
    if (!str)return str;
    while (EOF != (ch = fgetc(fp)) && ch != '\n') {
        str[len++] = (char) ch;
        if (len == size) {
            str = realloc(str, sizeof(char) * (size += 16));
            if (!str)return str;
        }
    }
    str[len++] = '\0';

    return realloc(str, sizeof(char) * len);
}

int main(int argc, char *argv[]) {
    // Always initialize the biome list before starting any seed finder or
    // biome generator.
    initBiomes();
    LayerStack g;

    // Translate the positions to the desired regions.
    int regPosX = 0;
    int regPosZ = 0;

    int mcversion = MC_1_7;
    const char *seedFileName;
    StructureConfig featureConfig;
    FILE *file = fopen("save.txt", "a");
    if (argc > 2) {
        if (sscanf(argv[1], "%d", &regPosX) != 1) regPosX = 0;
        if (sscanf(argv[2], "%d", &regPosZ) != 1) regPosZ = 0;

        if (argc > 3) {
            if (sscanf(argv[3], "%d", &mcversion) != 1) mcversion = MC_1_7;
        } else {
            printf("MC version not specified. Set using 'mcversion' argument:\n"
                   "17  for MC1.7 - MC1.12\n113 for MC1.13+\n"
                   "Defaulting to MC 1.7.\n\n");
            mcversion = MC_1_7;
        }
    } else {
        char *endptr;
        char *res;
        printf("Please input mc version: 1.7, 1.8, 1.9, 1.10, 1.11, 1.12, 1.13, 1.13.2, 1.14, 1.15\n");
        res = inputString(stdin, 20);
        mcversion = parse_version(res);
        if (mcversion == MC_LEG) {
            printf("You didnt use a correct version, defaulting to 1.7-1.12\n");
            mcversion = MC_1_7;
        }

        printf("Please input the relative X position you want the quad witch hut in your world (in blocks)\n");
        res = inputString(stdin, 20);
        errno = 0;
        long posX = strtol(res, &endptr, 10);
        if ((errno == ERANGE && (posX == LONG_MAX || posX == LONG_MIN))
            || (errno != 0 && posX == 0)) {
            perror("strtol");
            fprintf(stderr, "Outside of range\n");
            exit(EXIT_FAILURE);
        }
        if (endptr == res) {
            fprintf(stderr, "No digits were found\n");
            exit(EXIT_FAILURE);
        }
        printf("Please input the relative Z position you want the quad witch hut in your world (in blocks)\n");
        res = inputString(stdin, 20);
        errno = 0;
        long posZ = strtol(res, &endptr, 10);
        if ((errno == ERANGE && (posZ == LONG_MAX || posZ == LONG_MIN))
            || (errno != 0 && posZ == 0)) {
            perror("strtol");
            fprintf(stderr, "Outside of range\n");
            exit(EXIT_FAILURE);
        }
        if (endptr == res) {
            fprintf(stderr, "No digits were found\n");
            exit(EXIT_FAILURE);
        }
        regPosX = (int) (posX / 16 / 32);
        regPosZ = (int) (posZ / 16 / 32);
    }

    if (mcversion >= MC_1_13) {
        featureConfig = SWAMP_HUT_CONFIG;
        seedFileName = "./quadhutbases_1_13_Q1.txt";
        // setupGeneratorMC113() biome generation is slower and unnecessary.
        // We are only interested in the biomes on land, which haven't changed
        // since MC 1.7 except for some modified variants.
        g = setupGenerator(MC_1_7);
        // Use the 1.13 Hills layer to get the correct modified biomes.
        g.layers[L_HILLS_64].getMap = mapHills113;
    } else {
        featureConfig = FEATURE_CONFIG;
        seedFileName = "./quadhutbases_1_7_Q1.txt";
        g = setupGenerator(MC_1_7);
    }

    //seedFileName = "./seeds/quadbases_Q1b.txt";

    if (access(seedFileName, F_OK)) {
        printf("Seed base file does not exist: Creating new one.\n"
               "This may take a few minutes...\n");
        int threads = 6;
        int quality = 1;
        search4QuadBases(seedFileName, threads, featureConfig, quality);
    }

    int64_t i, j, qhcnt;
    int64_t base, seed;
    int64_t *qhcandidates = loadSavedSeeds(seedFileName, &qhcnt);


    Layer *lFilterBiome = &g.layers[L_BIOME_256];
    int *biomeCache = allocCache(lFilterBiome, 3, 3);


    // Load the positions of the four structures that make up the quad-structure
    // so we can test the biome at these positions.
    Pos qhpos[4];

    // Setup a dummy layer for Layer 19: Biome, to make preliminary seed tests.
    Layer layerBiomeDummy;
    setupLayer(256, &layerBiomeDummy, NULL, 200, NULL);


    int areaX = (regPosX << 1) + 1;
    int areaZ = (regPosZ << 1) + 1;

    fprintf(file,"Using version: %s at position %lld %lld (region: %d %d)\n",versions[mcversion],(long long)regPosX*16*32,(long long)regPosZ*16*32,regPosX,regPosZ);
    printf("Using version: %s at position %lld %lld (region: %d %d)\n",versions[mcversion],(long long)regPosX*16*32,(long long)regPosZ*16*32,regPosX,regPosZ);
    // Search for a swamp at the structure positions
    for (i = 0; i < qhcnt; i++) {
        base = moveStructure(qhcandidates[i], regPosX, regPosZ);

        qhpos[0] = getStructurePos(featureConfig, base, 0 + regPosX, 0 + regPosZ);
        qhpos[1] = getStructurePos(featureConfig, base, 0 + regPosX, 1 + regPosZ);
        qhpos[2] = getStructurePos(featureConfig, base, 1 + regPosX, 0 + regPosZ);
        qhpos[3] = getStructurePos(featureConfig, base, 1 + regPosX, 1 + regPosZ);

        /*
        for (j = 0; j < 4; j++)
        {
            printf("(%d,%d) ", qhpos[j].x, qhpos[j].z);
        }
        printf("\n");
        */

        // This little magic code checks if there is a meaningful chance for
        // this seed base to generate swamps in the area.
        // The idea is, that the conversion from Lush temperature to swamp is
        // independent of surroundings, so we can test for this conversion
        // beforehand. Furthermore, biomes tend to leak into the negative
        // coordinates because of the Zoom layers, so the majority of hits will
        // occur when SouthEast corner (at a 1:256 scale) of the quad-hut has a
        // swamp. (This assumption misses about 1 in 500 quad-hut seeds.)
        // Finally, here we also exploit that the minecraft random number
        // generator is quite bad, the "mcNextRand() mod 6" check has a period
        // pattern of ~3 on the high seed-bits, which means we can avoid
        // checking all 16 high-bit combinations.
        for (j = 0; j < 5; j++) {
            seed = base + ((j + 0x53) << 48);
            setWorldSeed(&layerBiomeDummy, seed);
            setChunkSeed(&layerBiomeDummy, areaX + 1, areaZ + 1);
            if (mcNextInt(&layerBiomeDummy, 6) == 5)
                break;
        }
        if (j >= 5)
            continue;


        int64_t hits = 0, swpc;

        for (j = 0; j < 0x10000; j++) {
            seed = base + (j << 48);

            /** Pre-Generation Checks **/
            // We can check that at least one swamp could generate in this area
            // before doing the biome generator checks.
            setWorldSeed(&layerBiomeDummy, seed);

            setChunkSeed(&layerBiomeDummy, areaX + 1, areaZ + 1);
            if (mcNextInt(&layerBiomeDummy, 6) != 5)
                continue;

            // This seed base does not seem to contain many quad huts, so make
            // a more detailed analysis of the surroundings and see if there is
            // enough potential for more swamps to justify searching further.
            if (hits == 0 && (j & 0xfff) == 0xfff) {
                swpc = 0;
                setChunkSeed(&layerBiomeDummy, areaX, areaZ + 1);
                swpc += mcNextInt(&layerBiomeDummy, 6) == 5;
                setChunkSeed(&layerBiomeDummy, areaX + 1, areaZ);
                swpc += mcNextInt(&layerBiomeDummy, 6) == 5;
                setChunkSeed(&layerBiomeDummy, areaX, areaZ);
                swpc += mcNextInt(&layerBiomeDummy, 6) == 5;

                if (swpc < (j > 0x1000 ? 2 : 1))
                    break;
            }

            // Dismiss seeds that don't have a swamp near the quad temple.
            setWorldSeed(lFilterBiome, seed);
            genArea(lFilterBiome, biomeCache, (regPosX << 1) + 2, (regPosZ << 1) + 2, 1, 1);

            if (biomeCache[0] != Swamp)
                continue;

            applySeed(&g, seed);
            if (getBiomeAtPos(g, qhpos[0]) != Swamp) continue;
            if (getBiomeAtPos(g, qhpos[1]) != Swamp) continue;
            if (getBiomeAtPos(g, qhpos[2]) != Swamp) continue;
            if (getBiomeAtPos(g, qhpos[3]) != Swamp) continue;


            fprintf(file, "%" PRId64 "\n", seed);
            printf("%" PRId64 "\n", seed);

            hits++;
        }
        fflush(file);
    }
    fclose(file);
    free(biomeCache);
    freeGenerator(g);

    return 0;
}
