#pragma once

#if defined(__SWITCH2__) && defined(ARISA_HORIZON_DEKO3D) && \
    defined(ARISA_HORIZON_VULKAN)
#include <arisa/provider/horizon/ounce/drako_console.hpp>
#include <arisa/provider/horizon/ounce/ounce_console.hpp>

namespace arisa::provider::horizon {

using Console = ounce::OunceConsole;

}
#elif defined(__SWITCH2__) && defined(ARISA_HORIZON_DEKO3D)
#include <arisa/provider/horizon/ounce/drako_console.hpp>

namespace arisa::provider::horizon {

using Console = ounce::DrakoConsole;

}
#elif defined(__SWITCH2__)
#include <arisa/provider/horizon/ounce/ounce_console.hpp>

namespace arisa::provider::horizon {

using Console = ounce::OunceConsole;

}
#else
#include <arisa/provider/horizon/nx/nx_console.hpp>

namespace arisa::provider::horizon {

using Console = nx::NxConsole;

}
#endif
