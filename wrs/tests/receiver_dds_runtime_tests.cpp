#include <receiver/client.hpp>

#include <cassert>
#include <cstdlib>

#ifndef DDS_TEST_PROFILE_PATH
#error "DDS_TEST_PROFILE_PATH must be defined by CMake."
#endif

int main() {
    receiver::Client client;
    unsetenv("WRS_DDS_PROFILE");
    const auto missing_profile = client.connect(0);
    assert(!missing_profile && missing_profile.error() == receiver::Error::InvalidArgument);
    setenv("WRS_DDS_PROFILE", "/does/not/exist.xml", 1);
    const auto nonexistent_profile = client.connect(0);
    assert(!nonexistent_profile && nonexistent_profile.error() == receiver::Error::InvalidArgument);
    assert(!client.is_connected());
}
