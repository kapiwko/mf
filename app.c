#include <curl/curl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "buf.h"
#include "log.h"
#include "jsmn.h"
#include "json.h"

const char DATA_DIR[] = "data";
const char URL[] = "https://wl-api.mf.gov.pl";

void createDataDirsIfNotExists(char *path)
{
    struct stat st = {0};
    if (stat(DATA_DIR, &st) == -1)
    {
        mkdir(DATA_DIR, 0700);
    }
    chdir(DATA_DIR);
}

int main(int argc, char **argv)
{
    char currentTime[20];
    char nip[11];
    char bankAccount[27];
    char date[11];
    char url[255];
    char status[20];
    char requestId[20];
    char code[20];
    char message[255];
    bool result = false;
    
    time_t now = time(NULL);
    strftime(currentTime, 20, "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    if (argc < 3)
    {
        printf("Missing parameters. Syntax: %s <nip> <bank-account> [date]\n", argv[0]);
        return -1;
    }
    
    if (strlen(argv[1]) != 10)
    {
        printf("NIP must have 10 characters.\n");
        return -1;
    }
    
    if (strlen(argv[2]) != 26)
    {
        printf("Bank account must have 26 characters.\n");
        return -1;
    }
    
    strcpy(nip, argv[1]);
    strcpy(bankAccount, argv[2]);
    
    if (argc > 3)
    {
        if (strlen(argv[3]) != 10) 
        {
            printf("Date must have 10 characters (YYYY-MM-DD).\n");
            return -1;
        }
        strcpy(date, argv[3]);
    } else {
        strftime(date, 20, "%Y-%m-%d", localtime(&now));
    }
    
    sprintf(url, "%s/api/check/nip/%s/bank-account/%s?%s", URL, nip, bankAccount, date);
    
    char *js = json_fetch(url);
    jsmntok_t *tokens = json_tokenise(js);
    
    typedef enum { START, KEY, STATUS, REQUEST_ID, CODE, MESSAGE, SKIP, STOP } parse_state;
    parse_state state = START;
    
    size_t object_tokens = 0;
    for (size_t i = 0, j = 1; j > 0; i++, j--)
    {
        jsmntok_t *t = &tokens[i];

        // Should never reach uninitialized tokens
        log_assert(t->start != -1 && t->end != -1);

        if (t->type == JSMN_ARRAY || t->type == JSMN_OBJECT) {
            j += t->size;
        }

        switch (state)
        {
            case START:
                if (t->type != JSMN_OBJECT)
                    log_die("Invalid response: root element must be an object.");

                state = KEY;
                object_tokens = t->size;

                if (object_tokens == 0)
                    state = STOP;

                if (object_tokens % 2 != 0)
                    log_die("Invalid response: object must have even number of children.");

                break;

            case KEY:
                object_tokens--;

                if (t->type != JSMN_STRING)
                    log_die("Invalid response: object keys must be strings.");

                state = SKIP;
                
                if (json_token_streq(js, t, "result"))
                {
                    result = true;
                    state = START;
                } 
                else if (json_token_streq(js, t, "accountAssigned"))
                {
                    state = STATUS;
                } 
                else if (json_token_streq(js, t, "requestId"))
                {
                    state = REQUEST_ID;
                }
                else if (json_token_streq(js, t, "code"))
                {
                    state = CODE;
                }
                else if (json_token_streq(js, t, "message"))
                {
                    state = MESSAGE;
                }

                break;

            case SKIP:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
                    log_die("Invalid response: object values must be strings or primitives.");

                object_tokens--;
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;

                break;

            case STATUS:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
                    log_die("Invalid response: object values must be strings or primitives.");
                
                sprintf(status, "%s", json_token_tostr(js, t));

                object_tokens--;
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;

                break;

            case REQUEST_ID:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
                    log_die("Invalid response: object values must be strings or primitives.");
                
                sprintf(requestId, "%s", json_token_tostr(js, t));

                object_tokens--;
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;

                break;

            case CODE:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
                    log_die("Invalid response: object values must be strings or primitives.");
                
                sprintf(code, "%s", json_token_tostr(js, t));

                object_tokens--;
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;

                break;

            case MESSAGE:
                if (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE)
                    log_die("Invalid response: object values must be strings or primitives.");
                
                sprintf(message, "%s", json_token_tostr(js, t));

                object_tokens--;
                state = KEY;

                if (object_tokens == 0)
                    state = STOP;

                break;

            case STOP:
                // Just consume the tokens
                break;

            default:
                log_die("Invalid state %u", state);
        }
    }
    
    createDataDirsIfNotExists(nip);
    
    char fileName[30];
    sprintf(fileName, "%s.txt", requestId);
    
    FILE *file = fopen(fileName, "w");
    
    fprintf(file, "%s: %s\n", "NOW", currentTime);
    fprintf(file, "%s: %s\n", "DATE", date);
    fprintf(file, "%s: %s\n", "NIP", nip);
    fprintf(file, "%s: %s\n", "BANK_ACCOUNT", bankAccount);
    
    if (result) {
        fprintf(file, "%s: %s\n", "STATUS", status);
        fprintf(file, "%s: %s\n", "REQUEST_ID", requestId);
    } else {
        fprintf(file, "%s: %s\n", "CODE", code);
        fprintf(file, "%s: %s\n", "MESSAGE", message);
    }
    
    fclose(file);
    
    printf("%s/%s\n", DATA_DIR, fileName);
        
    return 0;
}
