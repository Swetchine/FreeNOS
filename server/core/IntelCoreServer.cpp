/*
 * Copyright (C) 2020 Niek Linnenbank
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <FreeNOS/System.h>
#include <FreeNOS/API.h>
#include <Factory.h>
#include <Log.h>
#include "IntelCoreServer.h"

template<> CoreServer* AbstractFactory<CoreServer>::create()
{
    return new IntelCoreServer();
}

IntelCoreServer::IntelCoreServer()
    : CoreServer()
    , m_mp(m_apic)
{
}

IntelCoreServer::Result IntelCoreServer::initialize()
{
    // Register IPI vector. Minus 32 because of IRQ() in IntelKernel
    API::Result r = ProcessCtl(SELF, WatchIRQ, IPIVector - 32);

    if (r != API::Success)
    {
        ERROR("failed to register IPI vector: "
              "ProcessCtl(WatchIRQ) returned: " << (uint)r);
        return IOError;
    }

    return CoreServer::initialize();
}

IntelCoreServer::Result IntelCoreServer::bootCore(uint coreId, CoreInfo *info)
{
    // Signal the core to boot
    if (m_mp.boot(info) != IntelMP::Success)
    {
        ERROR("failed to boot core" << coreId);
        return BootError;
    }

    NOTICE("core" << coreId << " started");
    return Success;
}

IntelCoreServer::Result IntelCoreServer::discoverCores()
{
    m_mp.initialize();

    if (m_acpi.initialize() == IntelACPI::Success &&
        m_acpi.discover() == IntelACPI::Success)
    {
        NOTICE("using ACPI as CoreManager");
        m_cores = &m_acpi;
    }
    else if (m_mp.discover() == IntelMP::Success)
    {
        NOTICE("using MPTable as CoreManager");
        m_cores = &m_mp;
    }
    else
    {
        ERROR("no CoreManager found (ACPI or MPTable)");
        return NotFound;
    }

    return Success;
}

void IntelCoreServer::waitIPI() const
{
    // Wait for IPI which will wake us
    ProcessCtl(SELF, EnableIRQ, IPIVector);
    ProcessCtl(SELF, EnterSleep, 0, 0);
}

IntelCoreServer::Result IntelCoreServer::sendIPI(uint coreId)
{
    // Send IPI to ensure the slave wakes up for the message
    if (m_apic.sendIPI(coreId, IPIVector) != IntController::Success)
    {
        ERROR("failed to send IPI to core" << coreId);
        return IOError;
    }

    return Success;
}
