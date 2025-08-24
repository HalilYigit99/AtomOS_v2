#include "buffer.h"
#include "memory/heap.h"
#include "memory/memory.h"

// Create a new buffer with default data size
Buffer* buffer_create(size_t default_data_size) {
    Buffer* buffer = (Buffer*)malloc(sizeof(Buffer));
    if (!buffer) {
        return NULL;
    }
    
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->count = 0;
    buffer->total_size = 0;
    buffer->default_data_size = default_data_size;
    
    return buffer;
}

// Destroy buffer and free all nodes
void buffer_destroy(Buffer* buffer) {
    if (!buffer) return;
    
    buffer_clear(buffer);
    free(buffer);
}

// Clear all nodes from buffer
void buffer_clear(Buffer* buffer) {
    if (!buffer) return;
    
    BufferNode* current = buffer->head;
    while (current) {
        BufferNode* next = current->next;
        free(current);
        current = next;
    }
    
    buffer->head = NULL;
    buffer->tail = NULL;
    buffer->count = 0;
    buffer->total_size = 0;
}

// Push data to buffer (FIFO - back of queue)
int buffer_push(Buffer* buffer, const void* data) {
    if (!buffer || !data) return -1;
    
    size_t data_size = buffer->default_data_size;
    
    // Node + veri için yer ayır (flexible array member)
    BufferNode* new_node = (BufferNode*)malloc(sizeof(BufferNode) + data_size);
    if (!new_node) {
        return -1;
    }
    
    new_node->next = NULL;
    new_node->data_size = data_size;
    
    // Veriyi node'un data alanına kopyala
    memcpy(new_node->data, data, data_size);
    
    // Queue'ya ekle (tail'e ekle)
    if (buffer->count == 0) {
        buffer->head = new_node;
        buffer->tail = new_node;
    } else {
        buffer->tail->next = new_node;
        buffer->tail = new_node;
    }
    
    buffer->count++;
    buffer->total_size += data_size;
    return 0;
}

// Push data using default data size (artık gereksiz ama uyumluluk için)
int buffer_push_default(Buffer* buffer, const void* data) {
    return buffer_push(buffer, data);
}

// Pop data from buffer (FIFO - front of queue)
void* buffer_pop(Buffer* buffer) {
    if (!buffer || buffer->count == 0) {
        return NULL;
    }
    
    BufferNode* to_remove = buffer->head;
    void* data = to_remove->data;
    size_t data_size = to_remove->data_size;
    
    // Head'i güncelle
    buffer->head = buffer->head->next;
    if (buffer->count == 1) {
        buffer->tail = NULL;
    }
    
    buffer->count--;
    buffer->total_size -= data_size;
    
    // Not: Veri pointer'ını döndürüyoruz ama node'u silmiyoruz
    // Kullanıcı veriyi aldıktan sonra buffer_free_node() çağırmalı
    return data;
}

// Peek at front data without removing
void* buffer_peek(Buffer* buffer) {
    if (!buffer || buffer->count == 0) {
        return NULL;
    }
    
    return buffer->head->data;
}

// Get number of elements in buffer
size_t buffer_count(Buffer* buffer) {
    return buffer ? buffer->count : 0;
}

// Get total size of all data in buffer
size_t buffer_total_size(Buffer* buffer) {
    return buffer ? buffer->total_size : 0;
}

// Get data size per element in buffer
size_t buffer_data_size(Buffer* buffer) {
    return buffer ? buffer->default_data_size : 0;
}

// Check if buffer is empty
bool buffer_is_empty(Buffer* buffer) {
    return buffer ? (buffer->count == 0) : true;
}

// Push data with automatic memory copy (artık buffer_push ile aynı)
int buffer_push_copy(Buffer* buffer, const void* data) {
    return buffer_push(buffer, data);
}

// Pop entire node (kullanıcı node'u kendisi yönetecek)
BufferNode* buffer_pop_node(Buffer* buffer) {
    if (!buffer || buffer->count == 0) {
        return NULL;
    }
    
    BufferNode* to_remove = buffer->head;
    
    // Head'i güncelle
    buffer->head = buffer->head->next;
    if (buffer->count == 1) {
        buffer->tail = NULL;
    }
    
    buffer->count--;
    buffer->total_size -= to_remove->data_size;
    
    // Node'un bağlantısını kes
    to_remove->next = NULL;
    
    return to_remove;
}

// Free a buffer node
void buffer_free_node(BufferNode* node) {
    if (node) {
        free(node);
    }
}

// Iterator functions
BufferNode* buffer_iterator_begin(Buffer* buffer) {
    return buffer ? buffer->head : NULL;
}

BufferNode* buffer_iterator_next(BufferNode* current) {
    return current ? current->next : NULL;
}

void* buffer_node_data(BufferNode* node) {
    return node ? node->data : NULL;
}

size_t buffer_node_data_size(BufferNode* node) {
    return node ? node->data_size : 0;
}
