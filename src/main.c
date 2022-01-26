#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

#define CMD_RUN "run"
#define AUTHOR  "rphii"
#define GITHUB  "https://github.com/"AUTHOR"/p0c1p-Interpreter"
#define WIKI    "https://esolangs.org/wiki/)0,1("
#define VERSION "1.0.2"

#define HASH_SLOTS  0x1000

typedef struct P0c1p
{
    bool rotation;  // false = add; true = subtract
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

bool hash_get(P0c1p *state, double index, size_t *hash, size_t *i)
{
    if(!state || !i) return false;
    // hash origin
    uint64_t origin = *(uint64_t *)&index;
    size_t hash_scope = (origin * 0x61C88645) % HASH_SLOTS;
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

void rotate(P0c1p *state, double *value, double amount)
{
    if(!state || !value) return;
    double magnitude = fmod(amount, 1.0);
    state->someflow = true;
    if(state->rotation)
    {
        *value -= magnitude;
        if(*value < 0) *value += 1.0;
        else state->someflow = false;
    }
    else
    {
        *value += magnitude;
        if(*value > 1.0) *value -= 1.0;
        else state->someflow = true;
    }
    if(magnitude != amount) state->someflow = true;
}

uint64_t pow_int(uint64_t base, uint64_t exponent)
{
    uint64_t result = base;
    for(uint64_t i = 0; i < exponent; i++)
    {
        if(result * base < result) break;
        result *= base;
    }
    return result;
}

void run(char *str, size_t len)
{
    if(!str) return;
    char find = 0;
    bool stop = false;
    double at_i = 0;
    double at_j = 0;
    double temp = 0;
    P0c1p state = {0};
    state.j = 1.0;
    size_t i = 0;
    while(!stop && i < len)
    {
        if(str[i] == find && !(find = 0)) i++;
        switch(find)
        {
            case ']': { 
                i++;
            } continue;
            case '[': {
                i--;
            } continue;
            default: break;
        }
        switch(str[i])
        {
            case '+': {
                state.q++;
            } break;
            case '-': {
                state.q--;
            } break;
            case '^': {
                state.rotation = (state.rotation ^ true) & 1;
            } break;
            case '~': {
                temp = state.i;
                state.i = state.j;
                state.j = temp;
            } break;
            case '=': {
                if(!memory_get(&state, state.i, &at_i)) stop = true;
                if(!memory_get(&state, state.j, &at_j)) stop = true;
                rotate(&state, &at_i, at_j);
                if(!memory_set(&state, state.i, at_i)) stop = true;
            } break;
            case '\'': {
                if(!memory_get(&state, state.j, &state.i)) stop = true;
            } break;
            case '"': {
                if(!memory_set(&state, state.j, state.i)) stop = true;
            } break;
            case '*': {
                if(state.q < 0) temp = 1.0 / (double)pow_int(10, -state.q);
                else temp = (double)pow_int(10, state.q);
                rotate(&state, &state.i, temp);
            } break;
            case '[': {
                if(!state.someflow) find = ']';
            } break;
            case ']': {
                if(state.someflow) find = '[';
                i--;
            } break;
            case '.': {
                if(!memory_get(&state, state.i, &at_i)) stop = true;
                if(at_i == 0)
                {
                    stop = true;
                    break;
                }
                printf("%c", (int)round(1.0 / at_i));
            } break;
            case ',': {
                int input = getchar();
                if(input == 0)
                {
                    stop = true;
                    break;
                }
                temp = 1.0 / (double)input;
                if(!memory_set(&state, state.i, temp)) stop = true;
            } break;
            default: break;
        }
        i++;
    }
    memory_free(&state);
    if(stop)
    {
        printf("Some error occured...\n");
    }
}

int main(int argc, char **argv)
{
    if(argc == 1)
    {
        printf("Try -h\n");
    }
    for(int i = 1; i < argc; i++)
    {
        if(!argv[i]) continue;
        // check for switches
        if(argv[i][0] == '-')
        {
            switch(argv[i][1])
            {
                case 'h': {
                    printf("=== Available commands ===\n");
                    printf("-h\n\tlist this here\n\n");
                    printf("-i\n\tlist information\n\n");
                    printf("%s [filename]\n\trun a file\n\n", CMD_RUN);
                } break;
                case 'i': {
                    printf("=== Information ===\n");
                    printf("[0,1] or )0,1( or p0c1p Interpreter\n");
                    printf("Author: %s\n", AUTHOR);
                    printf("Version: %s\n", VERSION);
                    printf("Github: %s\n", GITHUB);
                    printf("Wiki: %s\n", WIKI);
                } break;
                default: break;
            }
        }
        else
        {
            // check for commands
            if(!strncmp(argv[i], CMD_RUN, sizeof(CMD_RUN)))
            {
                if(++i < argc)
                {
                    // run file
                    char *dump = 0;
                    size_t bytes = file_read(argv[i], &dump);
                    if(!bytes) printf("Could not open file '%s'.\n", argv[i]);
                    else run(dump, bytes);
                }
                else printf("Expected a filename.\n");
            }
            else
            {
                printf("Unkown command, try -h\n");
            }
        }
    }
}
