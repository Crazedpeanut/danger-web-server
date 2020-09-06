#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "request.h"

Request *init_request(char *method, char *path, char *version)
{
    Request *r = malloc(sizeof(Request));
    r->header = NULL;
    r->method = method;
    r->path = path;
    r->version = version;
    return r;
}

Header *init_header(char *key, char *value)
{
    Header *h = malloc(sizeof(Header));
    h->key = key;
    h->value = value;
    h->next = NULL;
    return h;
}

void push_header(Request *request, Header *header)
{
    header->next = request->header;
    request->header = header;
}

Header *pop_header(Request *request)
{
    Header *header = request->header;

    if (header == NULL)
    {
        return NULL;
    }

    request->header = header->next;
    return header;
}

void destroy_request(Request *request)
{
    if (request->header != NULL)
    {
        destroy_header(request->header);
    }

    free(request->method);
    free(request->path);
    free(request->version);
    free(request);
}

void destroy_header(Header *header)
{
    if (header->next != NULL)
    {
        destroy_header(header->next);
    }

    free(header->key);
    free(header->value);
    free(header);
}

Request *read_request(char *buffer)
{
    char *token = strtok(buffer, "\n");

    char *method = malloc(sizeof(char) * 20);
    char *version = malloc(sizeof(char) * 50);
    char *path = malloc(sizeof(char) * 1024);

    sscanf(token, "%19s %49s %1023s", method, path, version);
    Request *request = init_request(method, path, version);

    int offset = strlen(token) + 1;

    while (1)
    {
        token = strtok(buffer + offset, "\n");
        if (token == NULL || token[0] == '\r')
        {
            break;
        }

        char *key = malloc(sizeof(char) * 50);
        char *value = malloc(sizeof(char) * 1024);

        sscanf(token, "%49[^:]: %1023[^\r^\n]", key, value);

        Header *header = init_header(key, value);
        push_header(request, header);

        offset += strlen(token) + 1;
    }

    return request;
}

void print_request(Request *request)
{
    printf("%s %s %s\n", request->method, request->path, request->version);

    Header *header = request->header;
    while(header != NULL)
    {
        printf("%s %s\n", header->key, header->value);
        header = header->next;
    }
}