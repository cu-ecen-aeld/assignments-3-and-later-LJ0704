/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */
 
/*Reference :
ChatGPT: https://chatgpt.com/share/69a34751-6abc-8009-952e-870ae8b2ee38
*/

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
    
    //Checking pointer value to avoid dereferencing NULL pointer
    if(buffer == NULL || entry_offset_byte_rtn == NULL)
    {
    	return NULL;
    }
    
    size_t curr_pos = 0;
    uint8_t index;
    uint8_t count;
    
     /* Determine number of valid entries and starting index */
    if (buffer->full) {
        count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        index = buffer->out_offs;   /* Oldest entry */
    } else {
        count = buffer->in_offs;    /* Number of valid entries */
        index = 0;                  /* Oldest when not full */
    }

    for (uint8_t i = 0; i < count; i++) {

        struct aesd_buffer_entry *entry = &buffer->entry[index];

        if (char_offset < curr_pos + entry->size) {

            if (entry_offset_byte_rtn)
                *entry_offset_byte_rtn = char_offset - curr_pos;

            return entry;
        }

        curr_pos = curr_pos + entry->size;

        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    /* char_offset beyond total stored data */
    return NULL;
}

/**
 * @param buffer the buffer to compute the total size of
 * @return total number of bytes stored across all valid entries in the buffer
 */
size_t aesd_circular_buffer_total_size(struct aesd_circular_buffer *buffer)
{
    size_t total = 0;
    uint8_t count;
    uint8_t index;
    uint8_t i;

    if (buffer == NULL)
        return 0;

    if (buffer->full) {
        count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        index = buffer->out_offs;
    } else {
        count = buffer->in_offs;
        index = 0;
    }

    for (i = 0; i < count; i++) {
        total += buffer->entry[index].size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    return total;
}

/**
 * @param buffer  the circular buffer to seek within. Caller must hold any locks.
 * @param write_cmd  zero-referenced command index relative to the oldest stored entry
 * @param write_cmd_offset  zero-referenced byte offset within that command
 * @return the absolute file position (byte offset from start of all concatenated data)
 *         that corresponds to write_cmd/write_cmd_offset, or -1 if the parameters are
 *         out of range.
 */
ssize_t aesd_circular_buffer_fpos_from_cmd(struct aesd_circular_buffer *buffer,
                                           uint32_t write_cmd,
                                           uint32_t write_cmd_offset)
{
    uint8_t count;
    uint8_t index;
    uint8_t i;
    ssize_t  fpos = 0;

    if (buffer == NULL)
        return -1;

    /* How many entries are currently stored? */
    if (buffer->full) {
        count = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        index = buffer->out_offs;   /* oldest entry */
    } else {
        count = buffer->in_offs;
        index = 0;
    }

    /* write_cmd must refer to a stored entry */
    if (write_cmd >= count)
        return -1;

    /* Sum the sizes of all entries that come before write_cmd */
    for (i = 0; i < write_cmd; i++) {
        fpos += (ssize_t)buffer->entry[index].size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    /* Now `index` points to the target entry; validate the byte offset */
    if (write_cmd_offset >= buffer->entry[index].size)
        return -1;

    fpos += (ssize_t)write_cmd_offset;
    return fpos;
}


/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: Add entry to buffer, if buffer is full it will move out offest to overwrites the older entry
    */
    //Return, to avoid SEGV faults
    if((buffer == NULL) || (add_entry == NULL))
    {
    	return;
    }
    
    // 1. Add entry to buffer parameter in location buffer->in_offs
    buffer->entry[buffer->in_offs] = *add_entry;

    // increment in_offs wrap around
    buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    //2. If buffer is full, the 'out' offset must also move to skip the overwritten entry
    if (buffer->full) {
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // 3. Flag is set to true if the buffer has become full
    if (buffer->in_offs == buffer->out_offs) {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
