#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define COL_RD(msg) "\x1b[31;1m"msg "\x1b[0m"
#define COL_BL(msg) "\x1b[94;1m"msg "\x1b[0m"
#define COL_MG(msg) "\x1b[95;1m"msg "\x1b[0m"
#define COL_YL(msg) "\x1b[93;1m"msg "\x1b[0m"
#define COL_GR(msg) "\x1b[90;1m"msg "\x1b[0m"
#define COL_UL(msg) "\x1b[4m"msg "\033[0m"

#define AUTHOR  "rphii"
#define GITHUB  "https://github.com/"AUTHOR"/p0c1p-Interpreter"
#define WIKI    "https://esolangs.org/wiki/)0,1("
#define VERSION "1.5.0"

#define HASH_SLOTS  0x1000

typedef enum DebugLevel
{
    DEBUG_OFF,
    DEBUG_1,
    DEBUG_2,
    // keep last
    DEBUG_MAX = DEBUG_1,
}
DebugLevel;

typedef struct P0c1p
{
    DebugLevel debug;   // debug level
    bool someflow;  // false = no overflow and no underflow on last rotation
    int64_t q;      // 10^q
    double i;       // index 0 (main)
    double j;       // index 1 (secondary)
    struct          // the memory is essentially a hash table
    {
        size_t used;        // how many subslots are used
        uint64_t *origin;   // original indexing value
        double *value;      // actual value in memory
    }
    memory[HASH_SLOTS];
}
P0c1p;

size_t file_read(char *filename, char **dump)
{
    if(!filename || !dump) return 0;
    FILE *file = fopen(filename, "rb");
    if(!file) return 0;

    // get file length 
    fseek(file, 0, SEEK_END);
    size_t bytes_file = ftell(file);
    fseek(file, 0, SEEK_SET);

    // allocate memory
    char *temp = realloc(*dump, bytes_file + 1);
    if(!temp)
    {
        free(*dump);
        *dump = 0;
        return 0;
    }
    *dump = temp;
    
    // read file
    size_t bytes_read = fread(*dump, 1, bytes_file, file);
    if(bytes_file != bytes_read) return 0;
    (*dump)[bytes_read] = 0;
    
    // close file
    fclose(file);
    return bytes_read;
}

size_t file_write(char *filename, char *dump, size_t len)
{
    // open file
    FILE *file = fopen(filename, "wb");
    if(!file) return 0;

    // write file
    size_t result = fwrite(dump, sizeof(char), len, file);

    // close file
    fclose(file);

    return result;
}

uint64_t hash_generate(uint64_t in)
{
    in ^= UINT64_MAX;
    in *= 0x61C88645;
    in %= HASH_SLOTS;
    return in;
}

bool hash_get(P0c1p *state, double index, size_t *hash, size_t *i)
{
    if(!state || !i) return false;
    // hash origin
    uint64_t origin = *(uint64_t *)&index;
    size_t hash_scope = hash_generate(origin);
    if(hash) *hash = hash_scope;
    // check if the value exists
    bool found = false;
    for(*i = 0; *i < state->memory[hash_scope].used; (*i)++)
    {
        if(state->memory[hash_scope].origin[*i] == origin)
        {
            found = true;
            break;
        }
    }
    // if it doesn't exist, create it
    if(!found)
    {
        size_t used = state->memory[hash_scope].used;
        // get more memory
        uint64_t *temp_origin = realloc(state->memory[hash_scope].origin, sizeof(*state->memory[hash_scope].origin) * (used + 1));
        double *temp_value = realloc(state->memory[hash_scope].value, sizeof(*state->memory[hash_scope].value) * (used + 1));
        // safety check
        if(!temp_value || !temp_origin) return false;
        // set new memory
        state->memory[hash_scope].origin = temp_origin;
        state->memory[hash_scope].value = temp_value;
        // set index to where new memory is
        *i = used;
        // initialize new memory
        state->memory[hash_scope].origin[*i] = origin;
        state->memory[hash_scope].value[*i] = index;
        // increase used
        state->memory[hash_scope].used++;
    }
    return true;
}

bool memory_get(P0c1p *state, double index, double *out)
{
    if(!state) return false;
    // get hash
    size_t hash = 0;
    size_t i = 0;
    if(!hash_get(state, index, &hash, &i)) return false;
    // return value
    if(out) *out = state->memory[hash].value[i];
    return true;
}

bool memory_set(P0c1p *state, double index, double value)
{
    if(!state) return false;
    // get hash
    size_t hash = 0;
    size_t i = 0;
    if(!hash_get(state, index, &hash, &i)) return false;
    // set value
    state->memory[hash].value[i] = value;
    return true;
}

void memory_free(P0c1p *state)
{
    for(size_t i = 0; i < HASH_SLOTS; i++)
    {
        free(state->memory[i].origin);
        free(state->memory[i].value);
        state->memory[i].origin = 0;
        state->memory[i].value = 0;
        state->memory[i].used = 0;
    }
}

void rotate(P0c1p *state, double *value, double amount, bool rotation)
{
    if(!state || !value) return;
    double magnitude = amount > 1.0 ? fmod(amount, 1.0) : amount;
    state->someflow = true;
    if(rotation)
    {
        *value -= magnitude;
        if(*value < 0) *value += 1.0;
        else state->someflow = false;
    }
    else
    {
        *value += magnitude;
        if(*value > 1.0) *value -= 1.0;
        else state->someflow = false;
    }
    if(magnitude != amount) state->someflow = true;
}

uint64_t pow_int(uint64_t base, uint64_t exponent)
{
    uint64_t result = 1;
    for(uint64_t i = 0; i < exponent; i++)
    {
        if(result * base < result) break;
        result *= base;
    }
    return result;
}

size_t uncomment(char *str, size_t len)
{
    if(!str) return 0;
    size_t write = 0;
    for(size_t i = 0; i < len; i++) 
    {
        if(strpbrk(&str[i], "~+-'\"=:[].,") == &str[i]) str[write++] = str[i];
    }
    return write;
}

void run(P0c1p *state, char *str, size_t len)
{
    if(!state || !str) return;
    char find = 0;
    bool stop = false;
    double at_i = 0;
    double at_j = 0;
    double temp = 0;
    // initialize states
    size_t p_level = 0;
    state->someflow = false;
    state->q = 0;
    state->i = 0;
    state->j = 1.0;
    size_t i = 0;
    len = uncomment(str, len);
    while(!stop && i < len)
    {
        if(find)
        {
            if(str[i] == find && !--p_level)
            {
                find = 0;
                i++;
            }
            switch(find)
            {
                case ']': {
                    if(str[i] == '[') p_level++;
                    i++;
                } continue;
                case '[': {
                    if(str[i] == ']') p_level++;
                    i--;
                } continue;
                default: break;
            }
        }
        switch(str[i])
        {
            case '+': {
                state->q++;
            } break;
            case '-': {
                state->q--;
            } break;
            case '~': {
                // get @j from i
                if(!memory_get(state, state->i, &at_j)) stop = true;
                // get @i from j
                if(!memory_get(state, state->j, &at_i)) stop = true;
                // set @i
                if(!memory_set(state, state->i, at_i)) stop = true;
                // set @j
                if(!memory_set(state, state->j, at_j)) stop = true;
            } break;
            case '=': {
                // get @i
                if(!memory_get(state, state->i, &at_i)) stop = true;
                // rotate
                if(state->q < 0) temp = state->j / (double)pow_int(10, -state->q);
                else temp = state->j * (double)pow_int(10, state->q);
                rotate(state, &at_i, temp, false);
                // set @i
                if(!memory_set(state, state->i, at_i)) stop = true;
            } break;
            case ':': {
                // get @i
                if(!memory_get(state, state->i, &at_i)) stop = true;
                // rotate
                if(state->q < 0) temp = state->j / (double)pow_int(10, -state->q);
                else temp = state->j * (double)pow_int(10, state->q);
                rotate(state, &at_i, temp, true);
                // set @i
                if(!memory_set(state, state->i, at_i)) stop = true;
            } break;
            case '\'': {
                // get @i
                if(!memory_get(state, state->i, &at_i)) stop = true;
                // swap @i and i
                temp = at_i;
                at_i = state->i;
                state->i = temp;
                // set @i
                if(!memory_set(state, state->i, at_i)) stop = true;
            } break;
            case '"': {
                // get @j
                if(!memory_get(state, state->j, &at_j)) stop = true;
                // swap @j and j
                temp = at_j;
                at_j = state->j;
                state->j = temp;
                // set @j
                if(!memory_set(state, state->j, at_j)) stop = true;
            } break;
            case '[': {
                if(!state->someflow)
                {
                    find = ']';
                    p_level++;
                }
            } break;
            case ']': {
                if(state->someflow)
                {
                    find = '[';
                    p_level++;
                }
                i -= 2;
            } break;
            case '.': {
                if(!memory_get(state, state->i, &at_i)) stop = true;
                if(at_i == 0)
                {
                    stop = true;
                    break;
                }
                printf("%c", (int)round(1.0 / at_i));
                if(state->debug == DEBUG_1) printf(COL_BL(" << out ") COL_GR("'%c' = %f = 1/%lf") COL_BL(" <<\n"), (int)round(1.0 / at_i), 1.0 / at_i, at_i);
            } break;
            case ',': {
                int input = getchar();
                if(input == 0)
                {
                    stop = true;
                    break;
                }
                temp = 1.0 / (double)input;
                if(!memory_set(state, state->i, temp)) stop = true;
                if(state->debug == DEBUG_1) printf(COL_MG(">> in ") COL_GR("1/'%c' = 1/%d = %lf") COL_MG(" >>\n"), input, input, temp);
            } break;
            default: break;
        }
        i++;
    }
    memory_free(state);
    if(stop)
    {
        printf("Some error occured...");
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    if(argc == 1)
    {
        printf("Try -h\n");
    }
    P0c1p state = {0};
    char *dump = 0;
    for(int i = 1; i < argc; i++)
    {
        if(!argv[i]) continue;
        // check for switches
        if(argv[i][0] == '-')
        {
            if(argv[i][1] && argv[i][2])
            {
                printf("Unknown switch '%s'.\n", argv[i]);
                continue;
            }
            switch(argv[i][1])
            {
                case 'h': {
                    printf("=== Available commands ===\n");
                    printf("-h\n\tlist this here\n\n");
                    printf("-i\n\tlist information\n\n");
                    printf("-u [filename]\n\tuncomment a file\n\n");
                    printf("-d [number]\n\tset debug output level (%d = off ... %d = max)\n\n", DEBUG_OFF, DEBUG_MAX);
                    printf("-c [code]\n\trun passed code directly\n\n");
                    printf("-r [filename]\n\trun a file\n\n");
                } break;
                case 'i': {
                    printf("=== Information ===\n");
                    printf("[0,1] or )0,1( or p0c1p Interpreter\n");
                    printf("Author: %s\n", AUTHOR);
                    printf("Version: %s\n", VERSION);
                    printf("GitHub: %s\n", GITHUB);
                    printf("Wiki: %s\n", WIKI);
                } break;
                case 'u': {
                    if(++i < argc)
                    {
                        // uncomment a file
                        size_t bytes = file_read(argv[i], &dump);
                        if(!bytes) printf("Could not open file '%s'.\n", argv[i]);
                        else 
                        {
                            bytes = uncomment(dump, bytes);
                            file_write(argv[i], dump, bytes);
                        }
                        free(dump);
                        dump = 0;
                    }
                    else printf("Expected a filename.\n");
                } break;
                case 'd': {
                    if(++i < argc)
                    {
                        char *endptr;
                        DebugLevel level = strtol(argv[i], &endptr, 10);
                        if(*endptr == 0 && level >= DEBUG_OFF && level <= DEBUG_MAX)
                        {
                            state.debug = level;
                        }
                        else printf("Invalid debug level '%s'.\n", argv[i]);
                    }
                    else printf("Expected a number.\n");
                } break;
                case 'c': {
                    if(++i < argc)
                    {
                        run(&state, argv[i], strlen(argv[i]));
                    }
                    else printf("Expected code.\n");
                } break;
                case 'r':
                {
                    if(++i < argc)
                    {
                        // run file
                        size_t bytes = file_read(argv[i], &dump);
                        if(!bytes) printf("Could not open file '%s'.\n", argv[i]);
                        else run(&state, dump, bytes);
                        free(dump);
                        dump = 0;
                    }
                    else printf("Expected a filename.\n");
                }
                default: break;
            }
        }
        else printf("Unkown command '%s'\n", argv[i]);
    }
}

/* planned or in mind
- some sort of interactive switch, similar to python's "python" command
- improved debugging possibilities with breakpoints and memory viewer etc?
*/
