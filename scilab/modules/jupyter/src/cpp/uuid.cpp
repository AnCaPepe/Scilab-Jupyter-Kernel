#include "uuid.hxx"

extern "C"
{
#ifdef WIN32
#include <Rpc.h>
#else
#include <uuid/uuid.h>
#endif
}

std::string GenerateNewUUID()
{
#ifdef WIN32
  UUID uuid;
  UuidCreate( &uuid );

  unsigned char* str;
  UuidToStringA( &uuid, &str );

  std::string uuidString( (char*) str );

  RpcStringFreeA( &str );
#else
  uuid_t uuid;
  uuid_generate_random( uuid );
  char uuidString[ 37 ];
  uuid_unparse( uuid, uuidString );
#endif
    
  return uuidString;
}
