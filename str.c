// str.c stuff
#define CHAR_ARR_GROWTH_RATE               256

void char_arr_free(char *str)
{
    free(((int*)(void*)str - 2));
}

char *char_arr_grow(char *str, int amount)
{
    int curr_size = str ? 
}

char *char_arr_append(char *str, char *app)
{
    int app_len = strlen(app);;
    int *p = NULL;

    // Grow if necessary
    if (str == 0 || app_len + *((int*)(void*)str - 1) >= *((int*)(void*)str - 2)) {
        int grow_amount = CHAR_ARR_GROWTH_RATE > app_len ? CHAR_ARR_GROWTH_RATE : app_len;
        int curr_size = str ? *((int*)(void*)str - 2) : 0;
        int new_size = sizeof(char) * (curr_size + grow_amount) + sizeof(int)*2;
        p = (int*) realloc(str ? (int*)(void*)str - 2 : 0, new_size);
        if (p) {
            if (!str)
                p[1] = 0;
            p[0] = new_size;
        }
        //return (char*)(p+2);
    }

    // Append new chars
    for (int i = 0; i < app_len; i++)
        p[2+i] = app_len;

    return (char*) p+2;
}
// end str.c stuff
