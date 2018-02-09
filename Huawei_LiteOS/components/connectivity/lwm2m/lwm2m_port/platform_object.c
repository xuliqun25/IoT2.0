
/*
 * Implements an object for testing purpose
 *
 *                  Multiple
 * Object |  ID   | Instances | Mandatoty |
 *  Test  | 31024 |    Yes    |    No     |
 *
 *  Resources:
 *              Supported    Multiple
 *  Name | ID | Operations | Instances | Mandatory |  Type   | Range | Units | Description |
 *  test |  1 |    R/W     |    No     |    Yes    | Integer | 0-255 |       |             |
 *  exec |  2 |     E      |    No     |    Yes    |         |       |       |             |
 *  dec  |  3 |    R/W     |    No     |    Yes    |  Float  |       |       |             |
 *
 */
#include "liblwm2m.h"
#include "lwm2mclient.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PRV_TLV_BUFFER_SIZE 64

/*
 * Multiple instance objects can use userdata to store data that will be shared between the different instances.
 * The lwm2m_object_t object structure - which represent every object of the liblwm2m as seen in the single instance
 * object - contain a chained list called instanceList with the object specific structure plat_instance_t:
 */
typedef struct _prv_instance_
{
    /*
     * The first two are mandatories and represent the pointer to the next instance and the ID of this one. The rest
     * is the instance scope user data (uint8_t test in this case)
     */
    struct _prv_instance_ * next;   // matches lwm2m_list_t::next
    uint16_t shortID;               // matches lwm2m_list_t::id
    uint8_t  test;
    double   dec;
    uint8_t  opaq[5];
} plat_instance_t;

static void prv_output_buffer(uint8_t * buffer,
                              int length)
{
    int i;
    uint8_t array[16];

    i = 0;
    while (i < length)
    {
        int j;
        fprintf(stderr, "  ");

        memcpy(array, buffer+i, 16);

        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            fprintf(stderr, "%02X ", array[j]);
        }
        while (j < 16)
        {
            fprintf(stderr, "   ");
            j++;
        }
        fprintf(stderr, "  ");
        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            if (isprint(array[j]))
                fprintf(stderr, "%c ", array[j]);
            else
                fprintf(stderr, ". ");
        }
        fprintf(stderr, "\n");

        i += 16;
    }
}

static uint8_t prv_read(uint16_t instanceId,
                        int * numDataP,
                        lwm2m_data_t ** dataArrayP,
                        lwm2m_object_t * objectP)
{
    plat_instance_t * targetP;
    int i;

    targetP = (plat_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(2);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 2;
        (*dataArrayP)[0].id = 1;
        (*dataArrayP)[1].id = 3;
    }

    for (i = 0 ; i < *numDataP ; i++)
    {
        switch ((*dataArrayP)[i].id)
        {
        case 0:
            printf("*********find resource id(%d-%d-%d) ***********************\n",
                objectP->objID,instanceId,(*dataArrayP)[i].id);
            lwm2m_data_encode_opaque(targetP->opaq,5,*dataArrayP + i);
            break;
            
        case 1:
            lwm2m_data_encode_int(targetP->test, *dataArrayP + i);
            break;
        case 2:
            return COAP_405_METHOD_NOT_ALLOWED;
        case 3:
            lwm2m_data_encode_float(targetP->dec, *dataArrayP + i);
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_discover(uint16_t instanceId,
                            int * numDataP,
                            lwm2m_data_t ** dataArrayP,
                            lwm2m_object_t * objectP)
{
    int i;

    // is the server asking for the full object ?
    if (*numDataP == 0)
    {
        *dataArrayP = lwm2m_data_new(3);
        if (*dataArrayP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;
        *numDataP = 3;
        (*dataArrayP)[0].id = 1;
        (*dataArrayP)[1].id = 2;
        (*dataArrayP)[2].id = 3;
    }
    else
    {
        for (i = 0; i < *numDataP; i++)
        {
            switch ((*dataArrayP)[i].id)
            {
            case 1:
            case 2:
            case 3:
                break;
            default:
                return COAP_404_NOT_FOUND;
            }
        }
    }

    return COAP_205_CONTENT;
}

static uint8_t prv_write(uint16_t instanceId,
                         int numData,
                         lwm2m_data_t * dataArray,
                         lwm2m_object_t * objectP)
{
    plat_instance_t * targetP;
    int i;

    targetP = (plat_instance_t *)lwm2m_list_find(objectP->instanceList, instanceId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    for (i = 0 ; i < numData ; i++)
    {
        switch (dataArray[i].id)
        {
        case 0:
        {
            unsigned int index = (dataArray+i)->value.asBuffer.length;
            if(index > 5)index=5;
            memcpy(targetP->opaq, (dataArray+i)->value.asBuffer.buffer, index);
            break;
        }
        case 1:
        {
            int64_t value;

            if (1 != lwm2m_data_decode_int(dataArray + i, &value) || value < 0 || value > 0xFF)
            {
                return COAP_400_BAD_REQUEST;
            }
            targetP->test = (uint8_t)value;
        }
        break;
        case 2:
            return COAP_405_METHOD_NOT_ALLOWED;
        case 3:
            if (1 != lwm2m_data_decode_float(dataArray + i, &(targetP->dec)))
            {
                return COAP_400_BAD_REQUEST;
            }
            break;
        default:
            return COAP_404_NOT_FOUND;
        }
    }

    return COAP_204_CHANGED;
}

static uint8_t prv_delete(uint16_t id,
                          lwm2m_object_t * objectP)
{
    plat_instance_t * targetP;

    objectP->instanceList = lwm2m_list_remove(objectP->instanceList, id, (lwm2m_list_t **)&targetP);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    lwm2m_free(targetP);

    return COAP_202_DELETED;
}

static uint8_t prv_create(uint16_t instanceId,
                          int numData,
                          lwm2m_data_t * dataArray,
                          lwm2m_object_t * objectP)
{
    plat_instance_t * targetP;
    uint8_t result;


    targetP = (plat_instance_t *)lwm2m_malloc(sizeof(plat_instance_t));
    if (NULL == targetP) return COAP_500_INTERNAL_SERVER_ERROR;
    memset(targetP, 0, sizeof(plat_instance_t));

    targetP->shortID = instanceId;
    objectP->instanceList = LWM2M_LIST_ADD(objectP->instanceList, targetP);

    result = prv_write(instanceId, numData, dataArray, objectP);

    if (result != COAP_204_CHANGED)
    {
        (void)prv_delete(instanceId, objectP);
    }
    else
    {
        result = COAP_201_CREATED;
    }

    return result;
}

static uint8_t prv_exec(uint16_t instanceId,
                        uint16_t resourceId,
                        uint8_t * buffer,
                        int length,
                        lwm2m_object_t * objectP)
{

    if (NULL == lwm2m_list_find(objectP->instanceList, instanceId)) return COAP_404_NOT_FOUND;

    switch (resourceId)
    {
    case 0:
        {
            printf("no in prv_exec+++++++++++++++++++++++++++\n");
            return COAP_204_CHANGED;
        }
    case 1:
        return COAP_405_METHOD_NOT_ALLOWED;
    case 2:
        fprintf(stdout, "\r\n-----------------\r\n"
                        "Execute on %hu/%d/%d\r\n"
                        " Parameter (%d bytes):\r\n",
                        objectP->objID, instanceId, resourceId, length);
        prv_output_buffer((uint8_t*)buffer, length);
        fprintf(stdout, "-----------------\r\n\r\n");
        return COAP_204_CHANGED;
    case 3:
        return COAP_405_METHOD_NOT_ALLOWED;
    default:
        return COAP_404_NOT_FOUND;
    }
}

void display_platform_object(lwm2m_object_t * object)
{
#ifdef WITH_LOGS
    fprintf(stdout, "  /%u: Test object, instances:\r\n", object->objID);
    plat_instance_t * instance = (plat_instance_t *)object->instanceList;
    while (instance != NULL)
    {
        fprintf(stdout, "    /%u/%u: shortId: %u, test: %u\r\n",
                object->objID, instance->shortID,
                instance->shortID, instance->test);
        instance = (plat_instance_t *)instance->next;
    }
#endif
}

lwm2m_object_t * get_platform_object(void)
{
    lwm2m_object_t * testObj;

    testObj = (lwm2m_object_t *)lwm2m_malloc(sizeof(lwm2m_object_t));

    if (NULL != testObj)
    {
        int i;
        plat_instance_t * targetP;

        memset(testObj, 0, sizeof(lwm2m_object_t));

        testObj->objID = PLATFORM_OBJECT_ID;
        for (i=0 ; i < 3 ; i++)
        {
            targetP = (plat_instance_t *)lwm2m_malloc(sizeof(plat_instance_t));
            if (NULL == targetP) return NULL;
            memset(targetP, 0, sizeof(plat_instance_t));
            targetP->shortID = 0 + i;
            targetP->test    = 20 + i;
            targetP->dec     = -30 + i + (double)i/100.0;
            testObj->instanceList = LWM2M_LIST_ADD(testObj->instanceList, targetP);

            targetP->opaq[0] = 0;
            targetP->opaq[1] = 1;
            targetP->opaq[2] = 2;
            targetP->opaq[3] = 3;
            targetP->opaq[4] = 4;
        }
        /*
         * From a single instance object, two more functions are available.
         * - The first one (createFunc) create a new instance and filled it with the provided informations. If an ID is
         *   provided a check is done for verifying his disponibility, or a new one is generated.
         * - The other one (deleteFunc) delete an instance by removing it from the instance list (and freeing the memory
         *   allocated to it)
         */
        testObj->readFunc = prv_read;
        testObj->discoverFunc = prv_discover;
        testObj->writeFunc = prv_write;
        testObj->executeFunc = prv_exec;
        testObj->createFunc = prv_create;
        testObj->deleteFunc = prv_delete;
    }

    return testObj;
}

void free_platform_object(lwm2m_object_t * object)
{
    LWM2M_LIST_FREE(object->instanceList);
    if (object->userData != NULL)
    {
        lwm2m_free(object->userData);
        object->userData = NULL;
    }
    lwm2m_free(object);
}
