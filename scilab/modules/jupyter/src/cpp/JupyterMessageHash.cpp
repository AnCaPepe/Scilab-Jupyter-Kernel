/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2016 - Leonardo José Consoni
 *
 * Copyright (C) 2012 - 2016 - Scilab Enterprises
 *
 * This file is hereby licensed under the terms of the GNU GPL v2.0,
 * pursuant to article 5.3.4 of the CeCILL v.2.1.
 * This file was originally licensed under the terms of the CeCILL v2.1,
 * and continues to be available under such terms.
 * For more information, see the COPYING file which you should have received
 * along with this program.
 *
 */

#include "JupyterMessageHash.hxx"

#include <cstring>
#include <iostream>

MessageHashGenerator::MessageHashGenerator()
{
  digestEngine = NULL;
  enabled = false;
}

bool MessageHashGenerator::SetKey( std::string keyString, std::string hashAlgorithm )
{
  OpenSSL_add_all_digests();      // Enables the search for encryption engines
  
  enabled = false;
  
  key = keyString;
  
  if( hashAlgorithm.find( "hmac-" ) != std::string::npos )
  {
    std::string digestEngineName = hashAlgorithm.substr( hashAlgorithm.find_last_of( "-" ) + 1 );
    
    // Search for the encryption engine by its name
    if( (digestEngine = EVP_get_digestbyname( digestEngineName.c_str() )) != NULL )
      enabled = true;
  }
  
  return enabled;
}

std::string MessageHashGenerator::GenerateHash( std::string& header, std::string& parent, std::string& metadata, std::string& content )
{
  static HMAC_CTX digestContext;
  unsigned char hash[ 32 ];
  unsigned int hashLength;
  char hashString[ 65 ] = "";     // 64 characters + NULL terminator
  
  if( enabled )
  {
    HMAC_Init_ex( &digestContext, key.data(), key.size(), digestEngine, NULL );
    HMAC_Update( &digestContext, (const unsigned char*) header.data(), header.size() );
    HMAC_Update( &digestContext, (const unsigned char*) parent.data(), parent.size() );
    HMAC_Update( &digestContext, (const 
    unsigned char*) metadata.data(), metadata.size() );
    HMAC_Update( &digestContext, (const unsigned char*) content.data(), content.size() );
    HMAC_Final( &digestContext, hash, &hashLength );
    HMAC_CTX_cleanup( &digestContext );
    
    // Each byte is represented by 2 characters
    for( size_t byteIndex = 0; byteIndex < hashLength; byteIndex++ )
      sprintf( hashString + strlen( hashString ), "%02x", hash[ byteIndex ] );
  }
  
  return hashString;
}
