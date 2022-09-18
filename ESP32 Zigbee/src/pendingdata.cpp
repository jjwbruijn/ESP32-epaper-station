#include "pendingdata.h"

#include <vector>

#include <Arduino.h>

#include "settings.h"

std::vector<pendingdata*> pendingfiles;

void garbageCollection(void* parameter)
{
    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        pendingdata::garbageCollection();
    }
}

pendingdata* pendingdata::findByVer(uint64_t ver)
{
    for (int16_t c = 0; c < pendingfiles.size(); c++)
    {
        pendingdata* pending = nullptr;
        pending = pendingfiles.at(c);
        if (pending->ver == ver)
        {
            return pending;
        }
    }
    return nullptr;
}

void pendingdata::garbageCollection()
{
    for (int16_t c = 0; c < pendingfiles.size(); c++)
    {
        pendingdata* pending = pendingfiles.at(c);
        if (pending->timeout)
        {
            pending->timeout--;
        }
        else
        {
            if (pending->data != nullptr)
            {
                free(pending->data);
            }
            pending->data = nullptr;
            delete pending;
            pendingfiles.erase(pendingfiles.begin() + c);
        }
    }
}
