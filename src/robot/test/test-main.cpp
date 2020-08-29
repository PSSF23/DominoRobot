#define CATCH_CONFIG_RUNNER
#include <Catch/catch.hpp>
#include "constants.h"
#include "serial/SerialCommsFactory.h"

libconfig::Config cfg = libconfig::Config();

int main( int argc, char* argv[] ) 
{
    cfg.readFile(TEST_CONSTANTS_FILE);
    SerialCommsFactory::getFactoryInstance()->set_mode(SERIAL_FACTORY_MODE::MOCK);

    int result = Catch::Session().run(argc, argv);

    return result;
}