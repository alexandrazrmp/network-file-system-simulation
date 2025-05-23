#include "List.h"


//add entry to linked list
sync_info_mem_store* add_sync_entry(sync_info_mem_store** sync_list, const char* src, const char* tgt) {
    sync_info_mem_store* new_entry = malloc(sizeof(sync_info_mem_store));
    if (!new_entry) {
        perror("malloc failed");
        return *sync_list;  // return current head on failure
    }

    //initialize new entry
    strcpy(new_entry->source_dir, src);
    strcpy(new_entry->target_dir, tgt);
    new_entry->last_sync_time = 0;
    new_entry->active = 0;
    new_entry->error_count = 0;
    new_entry->next = NULL;

    //find the end of the list and add the new entry (or place it in the beginning if the list is empty)

    if (*sync_list == NULL) {
        *sync_list = new_entry;
        return *sync_list;
    }
    sync_info_mem_store* curr = *sync_list;
    while (curr->next != NULL) curr = curr->next;
    curr->next = new_entry;
    return *sync_list;
}

//check if entry exists in linked list
//has dual purpose, if target dir is NULL, it will return the entry with the source dir
//if target dir is not NULL, it will return the entry with the source dir and target dir if the match is corerct, else NULL

sync_info_mem_store* exists_sync_entry(sync_info_mem_store* sync_list, const char* src, const char* tgt) {
    sync_info_mem_store* current = sync_list;
    while (current != NULL) {
        if (strcmp(current->source_dir, src) == 0) {
            if (tgt == NULL) {
                return current; //entry exists, return target dir that is missing
            }
            //if target directory is given and pair matches, return the entry
            if (tgt != NULL && strcmp(current->target_dir, tgt) == 0) {
                return current;
            } 
            return NULL;  //error code, cannot have same source and different target
        }
        current = current->next;
    }
    return NULL;   // Does not exist
}


//delete entry from linked list using source directory
void delete_sync_entry(sync_info_mem_store** sync_list, const char* src) {
    sync_info_mem_store* current = *sync_list;
    sync_info_mem_store* prev = NULL;
    while (current != NULL) {
        if (strcmp(current->source_dir, src) == 0) {
            if (prev == NULL) {
                *sync_list = current->next;
            } else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}