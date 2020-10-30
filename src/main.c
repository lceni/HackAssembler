//
//  main.c
//  HakAssembler
//
//  Created by Ceni, Lucas on 19.10.20.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

char *trim(char *line);
int skipLine(char *line);
char *clearLine(char *line);
char *assembleLine(char *line);
char *resolveComp(char *comp);
char *resolveDest(char *dest);
char *resolveJump(char *jump);

const char* SYMBOLS[] = {"R0", "R1", "R2", "R3", "R4", "R5", "R6", "R7", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", "SCREEN", "KBD", "SP", "LCL", "ARG", "THIS", "THAT"};
const int VALUES[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16384, 24576, 0, 1, 2, 3, 4};
const unsigned int symbolsLength = sizeof(VALUES) / sizeof(int);

int numVars = 3000;
int maxVarSize = 50;
int varsLength = 0;
char** vars;
int* varPositions;

int numLabels = 1000;
int maxLabelSize = 50;
int labelsLength = 0;
char** labels;
int* lineNumbers;

int _DEBUG = 0;

/**
 *  Reads from input, assemble from Hack mnemonics to binary machine code, stores result in output
 */
int main(int argc, const char* argv[]) {
    if (argc < 3) {
        printf("Usage:\nHakAssembler <input_file> <output_file> [-d|--debug]\n");
        exit(0);
    }
    
    const char *inputFile = argv[1];
    const char *outputFile = argv[2];
    
    if (argc > 3) _DEBUG = strcmp(argv[3], "-d") == 0 || strcmp(argv[3], "--debug") == 0 ? 1 : 0;
    
    // open output in write "w" mode
    FILE* output = fopen(outputFile, "w");

    // open input in read "r" mode
    FILE* input = fopen(inputFile, "r");
    if (input == NULL) {
        perror("Unable to open input file");
        exit(-1);
    }
    
    // initialize labels & lineNumbers arrays
    lineNumbers = calloc(numLabels, sizeof(int));
    labels = malloc(numLabels * sizeof(char*));
    for (int i=0; i<numLabels; i++) {
        labels[i] = calloc(maxLabelSize + 1, sizeof(char));
    }
    
    // initialize vars & varPostions arrays
    varPositions = calloc(numVars, sizeof(int));
    vars = malloc(numVars * sizeof(char*));
    for (int i=0; i<numVars; i++) {
        vars[i] = calloc(maxVarSize + 1, sizeof(char));
    }

    // the initial size of file content in heap
    unsigned int initial_size = 100;

    // the current size of fileContent
    size_t fileContentBytes = initial_size * sizeof(char);

    // declare the memory pointer which will hold the file content
    char* fileContent = (char*) malloc(fileContentBytes);

    // declare a buffer which will be used while getting the file content
    char buffer[10];

    // declare the current position in fileContent
    size_t current = 0;

    if (_DEBUG) printf("Content during while:\n");
    while (fgets(buffer, 10, input) != NULL) {
        strcpy(fileContent + current, buffer);
        current += strlen(buffer);
        if (_DEBUG) printf("%s", buffer);

        size_t nextCurrentBytes = (current + 10) * sizeof(char);
        if (nextCurrentBytes >= fileContentBytes) {
            if (_DEBUG) printf("\n\nreallocating...\nnextCurrentBytes: %lu\nfileContentBytes: %lu\n", nextCurrentBytes, fileContentBytes);
            fileContentBytes += initial_size * sizeof(char);
            if (_DEBUG) printf("new fileContentBytes: %lu\n\n", fileContentBytes);
            fileContent = (char*) realloc(fileContent, fileContentBytes);
        }
    }

    if (_DEBUG) {
        printf("-----\n");
        printf("Total: %lu\nContent of fileContent:\n%s\n", current, fileContent);
        printf("-----\n");
    }
    
    // duplicate fileContent for the second pass
    char* fileContentCopy = malloc(fileContentBytes);
    memcpy(fileContentCopy, fileContent, fileContentBytes);

    // iterate over the file content
    char* delimiter = "\n";
    char* currLine = strtok(fileContent, delimiter);
    int lineNumber = 0;
    
    // first pass
    while (currLine != NULL) {
        currLine = trim(currLine);
        
        // search for labels
        if (*currLine == '(') {
            // clear chars '(' and ')'
            char *replace;
            if ((replace  = strchr(currLine, ')'))) *replace = '\0';
            
            // mark line as label
            lineNumbers[labelsLength] = lineNumber;
            strcpy(labels[labelsLength++], currLine+1);
        }
        
        // search for variables
        if (*currLine == '@') {
            int isSymbol = 0;
            for (int i=0; i<symbolsLength; i++) {
                if (strcmp(currLine + 1, SYMBOLS[i]) == 0) {
                    isSymbol = 1;
                    break;
                }
            }
            
            if (!isSymbol && isalpha(*(currLine+1))) {
                strcpy(vars[varsLength++], currLine + 1);
            }
        }
        
        if (!skipLine(currLine)) lineNumber++;
        currLine = strtok(NULL, delimiter);
    }
    
    // before second pass, adjust vars by removing labels
    for (int i=0; i<varsLength; i++) {
        // remove vars found in labels
        for (int j=0; j<labelsLength; j++) {
            if (vars[i] && strcmp(vars[i], labels[j]) == 0) {
                free(vars[i]);
                vars[i] = NULL;
                break;
            }
        }
        
        // remove duplicated vars
        for (int j=i+1; j<varsLength; j++) {
            if (vars[i] && vars[j] && strcmp(vars[i], vars[j]) == 0) {
                free(vars[j]);
                vars[j] = NULL;
            }
        }
    }
    
    // now adjust the varPositions
    int varPosition = 0;
    for (int i=0; i<varsLength; i++) {
        if (vars[i]) {
            varPositions[i] = 16 + varPosition++; // R0-R15 is occupied, therefore variables starts at RAM[16]
        }
    }
    
    // second pass
    currLine = strtok(fileContentCopy, delimiter);
    while (currLine != NULL) {
        currLine = trim(currLine);
        if (!skipLine(currLine)) {
            char *lineToSave = assembleLine(clearLine(currLine));
            if (_DEBUG) printf("%s\n", lineToSave);
            fputs(lineToSave, output);
            fputc('\n', output);

            free(lineToSave);
        }
        currLine = strtok(NULL, delimiter);
    }
    
    if (_DEBUG) {
        printf("Labels (%d):\n", labelsLength);
        for (int i=0; i<labelsLength; i++) {
            printf("%s -> %d\n", labels[i], lineNumbers[i]);
        }
    }
    
    if (_DEBUG) {
        printf("Vars (%d):\n", varsLength);
        for (int i=0; i<varsLength; i++) {
            if (vars[i]) printf("%s -> %d\n", vars[i], varPositions[i]);
        }
    }

    // free everything
    free(fileContent);
    free(fileContentCopy);
    fclose(input);
    fclose(output);
    
    for (int i=0; i<numLabels; i++) free(labels[i]);
    for (int i=0; i<numVars; i++) free(vars[i]);
    free(lineNumbers);
    free(labels);
    free(vars);
    free(varPositions);
    
    return 0;
}

/**
 *  Whether skips the line because of comments, empty line or labels
 */
int skipLine(char* line) {
    if (*line == '\0') return 1;
    if (isspace(*line)) return 1;
    if (strlen(line) > 1 && *line == '/' && *(line + 1) == '/') return 1;
    if (*line == '(') return 1;
    return 0;
}

/**
 *  Trims the beginning of the string pointed by *line.
 */
char* trim(char* line) {
    if (line == NULL) return line;

    int i=0;
    while(isspace(*(line + i))) {
        i++;
    }
    
    char *replace;
    if ((replace  = strchr(line, '\r'))) *replace = '\0';
    if ((replace  = strchr(line, '\n'))) *replace = '\0';
    
    return line + i;
}

/**
 *  Removes comments and carrier returns(\r & \n) if any
 */
char* clearLine(char* line) {
    int backslash=-1;
    for (int i=0; i<strlen(line); i++) {
        if (*(line + i) == '/') backslash++;
        if (backslash) {
            *(line + i - 1) = '\0';
            break;
        }
    }
    
    return line;
}

/**
 *  Assemble the current line by converting dest, comp and jump components into a binary representation containing 16 chars varying from 0 to 1.
 */
char* assembleLine(char* line) {
    if (line == NULL) return line;
    
    char *binary = calloc(17, sizeof(char));
    strcpy(binary, "0000000000000000");
    if (*line == '@') {
        // this is an A instruction
        short value = -1;
        
        // check for symbols
        for (int i=0; i<symbolsLength; i++) {
            if (strcmp(line+1, SYMBOLS[i]) == 0) {
                // found a symbol, convert the line to the corresponding value
                value = VALUES[i];
                break;
            }
        }
        
        // check for labels
        if (value < 0) {
            for (int i=0; i<labelsLength; i++) {
                if (strcmp(line+1, labels[i]) == 0) {
                    // found a label, convert the line to the corresponding line number value
                    value = lineNumbers[i];
                    break;
                }
            }
        }
        
        // check for vars
        if (value < 0) {
            for (int i=0; i<varsLength; i++) {
                if (vars[i] && strcmp(line+1, vars[i]) == 0) {
                    // found a label, convert the line to the corresponding line number value
                    value = varPositions[i];
                    break;
                }
            }
        }
        
        // then it's a conversible value
        if (value < 0) {
            value = atoi(line + 1);
        }
        
        // ultimately convert value to binary representation
        for (int i=15; i>0; i--) {
            *(binary + i) = value % 2 == 0 ? '0' : '1';
            value = value / 2;
        }
        if (strcmp(binary, "0000000011110100") == 0) {
            printf("found!");
        }
    } else {
        // this is a C instruction
        *(binary + 0) = '1';
        *(binary + 1) = '1';
        *(binary + 2) = '1';
        
        char *dest=NULL, *comp=NULL, *jump=NULL;
        if ((dest = strchr(line, '='))) {
            *dest = '\0'; // breaks the line into dest
            dest = line;
            line = line + strlen(dest) + 1;
        }
        
        if ((jump = strchr(line, ';'))) {
            *jump = '\0';
            jump = jump + 1;
        }
        
        comp = line;
        
        // discover 'a'
        *(binary + 3) = strchr(comp, 'M') ? '1' : '0';
        
        // discover 'comp'
        char *resolvedComp = resolveComp(comp);
        strcpy(binary + 4, resolvedComp);
        
        // discover 'dest'
        char *resolvedDest = resolveDest(dest);
        strcpy(binary + 10, resolvedDest);
        
        // discover 'jump'
        char *resolvedJump = resolveJump(jump);
        strcpy(binary + 13, resolvedJump);
        
        if (_DEBUG) printf("dest: %s, comp: %s, jump: %s\n", dest, comp, jump);
    }
    
    return binary;
}

/**
 *  Resolves the compute (comp) part of a Hack instruction
 */
char *resolveComp(char *comp) {
    if (strcmp(comp, "0") == 0) return "101010";
    else if (strcmp(comp, "1") == 0) return "111111";
    else if (strcmp(comp, "-1") == 0) return "111010";
    else if (strcmp(comp, "D") == 0) return "001100";
    else if (strcmp(comp, "A") == 0 || strcmp(comp, "M") == 0) return "110000";
    else if (strcmp(comp, "!D") == 0) return "001101";
    else if (strcmp(comp, "!A") == 0 || strcmp(comp, "!M") == 0) return "110001";
    else if (strcmp(comp, "-D") == 0) return "001111";
    else if (strcmp(comp, "-A") == 0) return "110011";
    else if (strcmp(comp, "D+1") == 0) return "011111";
    else if (strcmp(comp, "A+1") == 0 || strcmp(comp, "M+1") == 0) return "110111";
    else if (strcmp(comp, "D-1") == 0) return "001110";
    else if (strcmp(comp, "A-1") == 0 || strcmp(comp, "M-1") == 0) return "110010";
    else if (strcmp(comp, "D+A") == 0 || strcmp(comp, "D+M") == 0) return "000010";
    else if (strcmp(comp, "D-A") == 0 || strcmp(comp, "D-M") == 0) return "010011";
    else if (strcmp(comp, "A-D") == 0 || strcmp(comp, "M-D") == 0) return "000111";
    else if (strcmp(comp, "D&A") == 0 || strcmp(comp, "D&M") == 0) return "000000";
    else if (strcmp(comp, "D|A") == 0 || strcmp(comp, "D|M") == 0) return "010101";
    return "000000";
}

/**
 *  Resolves the destination dest) part of a Hack instruction
 */
char *resolveDest(char *dest) {
    if (dest == NULL) return "000";
    else if (strcmp(dest, "M") == 0) return "001";
    else if (strcmp(dest, "D") == 0) return "010";
    else if (strcmp(dest, "MD") == 0) return "011";
    else if (strcmp(dest, "A") == 0) return "100";
    else if (strcmp(dest, "AM") == 0) return "101";
    else if (strcmp(dest, "AD") == 0) return "110";
    else if (strcmp(dest, "AMD") == 0) return "111";
    
    return "000";
}

/**
 *  Resolves the jump (jump) part of a Hack instruction
 */
char *resolveJump(char *jump) {
    if (jump == NULL) return "000";
    else if (strcmp(jump, "JGT") == 0) return "001";
    else if (strcmp(jump, "JEQ") == 0) return "010";
    else if (strcmp(jump, "JGE") == 0) return "011";
    else if (strcmp(jump, "JLT") == 0) return "100";
    else if (strcmp(jump, "JNE") == 0) return "101";
    else if (strcmp(jump, "JLE") == 0) return "110";
    else if (strcmp(jump, "JMP") == 0) return "111";
    
    return "000";
}
