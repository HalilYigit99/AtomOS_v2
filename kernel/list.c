#include <memory/memory.h>
#include <list.h>

void List_Init(List* list) {
	if (!list) return;
	list->head = NULL;
	list->tail = NULL;
	list->count = 0;
}

List* List_Create(void) {
	List* list = (List*)malloc(sizeof(List));
	if (!list) return NULL;
	List_Init(list);
	return list;
}

void List_Destroy(List* list, bool freeData) {
	if (!list) return;
	List_Clear(list, freeData);
	free(list);
}

void List_Add(List* self, void* item) {
	if (!self) return;
	ListNode* node = (ListNode*)malloc(sizeof(ListNode));
	if (!node) return; // out of memory, sessizce düş
	node->data = item;
	node->next = NULL;

	if (!self->head) {
		self->head = self->tail = node;
	} else {
		self->tail->next = node;
		self->tail = node;
	}
	self->count++;
}

bool List_RemoveAt(List* self, size_t index) {
	if (!self) return false;
	if (index >= self->count) return false;

	ListNode* prev = NULL;
	ListNode* cur = self->head;
	for (size_t i = 0; i < index; ++i) {
		prev = cur;
		cur = cur->next;
	}

	if (!prev) {
		// head siliniyor
		self->head = cur->next;
		if (self->tail == cur) self->tail = cur->next; // tek eleman durumunda NULL olur
	} else {
		prev->next = cur->next;
		if (self->tail == cur) self->tail = prev;
	}

	free(cur);
	self->count--;
	return true;
}

bool List_RemoveAtIndex(List* self, size_t index) {
	return List_RemoveAt(self, index);
}

bool List_Remove(List* self, void* item) {
	if (!self) return false;
	ListNode* prev = NULL;
	ListNode* cur = self->head;
	while (cur) {
		if (cur->data == item) {
			if (!prev) {
				self->head = cur->next;
				if (self->tail == cur) self->tail = cur->next; // tek eleman -> NULL
			} else {
				prev->next = cur->next;
				if (self->tail == cur) self->tail = prev;
			}
			free(cur);
			self->count--;
			return true;
		}
		prev = cur;
		cur = cur->next;
	}
	return false;
}

int64_t List_IndexOf(List* self, void* item) {
	if (!self) return -1;
	size_t idx = 0;
	for (ListNode* n = self->head; n; n = n->next, ++idx) {
		if (n->data == item) return (int64_t)idx;
	}
	return -1;
}

void* List_GetAt(List* self, size_t index) {
	if (!self) return NULL;
	if (index >= self->count) return NULL;
	ListNode* n = self->head;
	for (size_t i = 0; i < index; ++i) n = n->next;
	return n ? n->data : NULL;
}

bool List_InsertAt(List* self, size_t index, void* item) {
	if (!self) return false;
	if (index > self->count) return false; // index == count -> sona ekle

	if (index == self->count) {
		List_Add(self, item);
		return true;
	}

	ListNode* node = (ListNode*)malloc(sizeof(ListNode));
	if (!node) return false;
	node->data = item;

	if (index == 0) {
		node->next = self->head;
		self->head = node;
		if (!self->tail) self->tail = node;
		self->count++;
		return true;
	}

	ListNode* prev = self->head;
	for (size_t i = 0; i < index - 1; ++i) prev = prev->next;
	node->next = prev->next;
	prev->next = node;
	if (node->next == NULL) self->tail = node;
	self->count++;
	return true;
}

void List_Clear(List* self, bool freeData) {
	if (!self) return;
	ListNode* n = self->head;
	while (n) {
		ListNode* next = n->next;
		if (freeData && n->data) {
			free(n->data);
		}
		free(n);
		n = next;
	}
	self->head = self->tail = NULL;
	self->count = 0;
}

size_t List_Size(List* list)
{
	return list ? list->count : 0;
}

bool List_IsEmpty(List* list)
{
	return list ? list->count == 0 : true;
}

ListNode* List_Foreach_Begin(List* list)
{
	if (!list || list->count == 0) return NULL;
	return list->head;
}

ListNode* List_Foreach_Next(ListNode* node)
{
	if (!node) return NULL;
	return node->next;
}

void* List_Foreach_Data(ListNode* node)
{
	if (!node) return NULL;
	return node->data;
}
