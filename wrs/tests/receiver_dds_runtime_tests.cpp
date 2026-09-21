#include <receiver/dds_runtime.hpp>

#include <cassert>
#include <cstdlib>

#ifndef DDS_TEST_PROFILE_PATH
#error "DDS_TEST_PROFILE_PATH must be defined by CMake."
#endif

int main() {
    receiver::DdsRuntime runtime{};

    const auto empty_profile = runtime.initialize("");
    assert(!empty_profile);
    assert(empty_profile.error() == receiver::Error::InvalidArgument);
    assert(!runtime.is_initialized());

    const auto initialized = runtime.initialize(DDS_TEST_PROFILE_PATH);
    assert(initialized);
    assert(runtime.is_initialized());

    const auto repeated_initialization = runtime.initialize(DDS_TEST_PROFILE_PATH);
    assert(!repeated_initialization);
    assert(repeated_initialization.error() == receiver::Error::AlreadyInitialized);

    // ddsWrapperUninit may wait indefinitely for the shared executor when a
    // standalone process initializes DDS without creating a Node. Let process
    // teardown reclaim the test-only runtime after validating the API contract.
    std::_Exit(0);
}
