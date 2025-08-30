#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

// Basit, tek yönlü, pointer tutan liste düğümü
typedef struct ListNode {
	struct ListNode* next;
	void* data;
} ListNode;

typedef struct List {
	ListNode* head;
	ListNode* tail;
	size_t    count;
} List;

// Ömür yönetimi (global fonksiyonlar)
List*  List_Create(void);                        // Heap üzerinde yeni liste
void   List_Destroy(List* list, bool freeData);  // Clear + list'in kendisini free eder

// Operasyonlar (C# List benzeri)
void     List_Add(List* list, void* item);
bool     List_Remove(List* list, void* item);           // İlk eşleşmeyi (pointer eşitliği) siler
bool     List_RemoveAt(List* list, size_t index);       // Index out-of-range -> false
bool     List_RemoveAtIndex(List* list, size_t index);  // Alias: RemoveAtIndex
int64_t  List_IndexOf(List* list, void* item);          // Yoksa -1
void*    List_GetAt(List* list, size_t index);          // Out-of-range -> NULL
bool     List_InsertAt(List* list, size_t index, void* item);
void     List_Clear(List* list, bool freeData);         // Eleman pointer'larını da free etmek isterseniz freeData=true
size_t   List_Size(List* list);                     // Liste boyutu
bool     List_IsEmpty(List* list);                  // Liste boş mu?

ListNode* List_Foreach_Begin(List* list);  // Başlangıç düğümünü alır
ListNode* List_Foreach_Next(ListNode* node); // Sonraki düğümü alır
void*     List_Foreach_Data(ListNode* node); // Düğümün verisini alır

// Basit gezinme yardımcısı (opsiyonel)
#define LIST_FOR_EACH(listPtr, itVar) for (ListNode* itVar = (listPtr)->head; itVar != NULL; itVar = itVar->next)

#ifdef __cplusplus
}
#endif

