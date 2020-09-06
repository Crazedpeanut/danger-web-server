#ifndef REQUEST_H
#define REQUEST_H

typedef struct Header
{
    char *key;
    char *value;

    struct Header *next;
} Header;

typedef struct Request
{
    char *method;
    char *path;
    char *version;

    struct Header *header;
} Request;

Request* init_request(char *method, char* path, char *version);
Header* init_header(char *key, char *value);
void push_header(Request *request, Header *header);
Header* pop_header(Request *request);

void destroy_request(Request *request);
void destroy_header(Header *header);

Request *read_request(char *buffer);
void print_request(Request *request);

#endif // REQUEST_H