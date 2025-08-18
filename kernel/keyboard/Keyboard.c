#include <keyboard/Keyboard.h>
#include <list.h>
#include <debug/debug.h>
#include <util/string.h>

KeyboardLayouts currentLayout = LAYOUT_US_QWERTY; // Default keyboard layout

List* keyboardInputStreamList = NULL; // Global list to hold keyboard input streams given from drivers

static bool isOpen = false; // Flag to check if the keyboard input stream is open

bool __kbd_abstraction_initialized;

static int __open();
static void __close();
static int __readChar(char* c);
static int __readString(char* str, size_t maxLength);
static int __readBuffer(void* buffer, size_t size);
static int __available();
static char __peek();
static void __flush();

InputStream keyboardInputStream = {
    .Open = __open, // Define Open function
    .Close = __close, // Define Close function
    .readChar = __readChar, // Define readChar function
    .readString = __readString, // Define readString function
    .readBuffer = __readBuffer, // Define readBuffer function
    .available = __available, // Define available function
    .peek = __peek, // Define peek function
    .flush = __flush // Define flush function
};

static int __open() {
    if (isOpen) {
        WARN("Keyboard input stream is already open.\n");
        return -1; // Already open
    }

    keyboardInputStreamList = List_Create(); // Create a new list for keyboard input streams

    if (!keyboardInputStreamList) {
        return -1; // Failed to create list
    }

    isOpen = true; // Set the open flag
    __kbd_abstraction_initialized = true; // Set the keyboard abstraction layer initialized flag

    return 0; // Return 0 on success
}

static void __close() {
    // Clean up resources used by the keyboard input stream

    if (!isOpen) {
        return; // Already closed
    }

    List_Destroy(keyboardInputStreamList, true); // Clear the list and free its data

    keyboardInputStreamList = NULL; // Set the list to NULL

    __kbd_abstraction_initialized = false; // Reset the keyboard abstraction layer initialized flag

}

static int __readChar(char* c) {
    // Read a single character from the keyboard input stream
    // This is a placeholder implementation
    
    if (!isOpen || !c) {
        *c = '\0'; // Set character to null if not open or invalid pointer
        return -1; // Not open or invalid pointer
    }

    if (List_Size(keyboardInputStreamList) == 0) {
        *c = '\0'; // No characters available, set to null
        return -2; // No character read
    }

    for (ListNode* node = List_Foreach_Begin(keyboardInputStreamList); node != NULL; node = List_Foreach_Next(node)) {
        InputStream* stream = (InputStream*)node->data; // Get the InputStream from the node
        if (stream->available() > 0) {
            char character;
            if (stream->readChar(&character) == 1) { // Read a character
                *c = character; // Set the character to the output pointer
                return 1; // Return 1 on success
            }
        }
    }

    *c = '\0'; // No pressed key found, set to null
    return -3; // No pressed key found

}

static int __readString(char* str, size_t maxLength) {
    
    if (!isOpen || !str || maxLength == 0) {
        str[0] = '\0'; // Set string to empty if not open or invalid pointer
        return -1; // Not open or invalid pointer
    }

    if (List_Size(keyboardInputStreamList) == 0) {
        str[0] = '\0'; // No characters available, set to empty
        return -2; // No string read
    }

    for (size_t length = 0; length < maxLength; length++) {
        if (__readChar(&str[length]) != 1) {
            // Failed to read a character
            break;
        }
    }

    str[maxLength - 1] = '\0'; // Null-terminate the string
    return strlen(str); // Return the length of the string read
}

static int __readBuffer(void* buffer, size_t size) {
    return __readString((char*)buffer, size); // Read a string into the buffer
}

static int __available() {
    // Check if there are characters available to read from the keyboard input stream
    // This is a placeholder implementation
    int availableCount = 0;

    // Check all input streams in the list
    for (ListNode* node = List_Foreach_Begin(keyboardInputStreamList); node != NULL; node = List_Foreach_Next(node)) {
        InputStream* stream = (InputStream*)node->data; // Get the InputStream from the node
        availableCount += stream->available(); // Add the available count from each stream
    }

    return availableCount; // Return the total available count

}

static char __peek() {
    // Peek at the next character in the keyboard input stream without removing it
    // This is a placeholder implementation
    
    if (!isOpen) {
        return '\0'; // Not open, return null character
    }

    for (ListNode* node = List_Foreach_Begin(keyboardInputStreamList); node != NULL; node = List_Foreach_Next(node)) {
        InputStream* stream = (InputStream*)node->data; // Get the InputStream from the node
        if (stream->available() > 0) {
            return stream->peek(); // Return the next character without removing it
        }
    }

    return 0; // Return 0 as a placeholder, no actual peek implementation
}

static void __flush() {
    // Flush the keyboard input stream
    // This is a placeholder implementation
    // In a real implementation, this would clear any buffered input
}
