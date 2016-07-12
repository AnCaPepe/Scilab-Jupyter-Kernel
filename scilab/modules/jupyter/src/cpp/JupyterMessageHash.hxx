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

/**
 ** \file JupyterMessageHash.hxx
 ** \brief Define class JupyterMessageHash.
 */

#ifndef MESSAGE_HASH_GENERATOR_HXX
#define MESSAGE_HASH_GENERATOR_HXX

#include <openssl/hmac.h>
#include <string>

/**
 ** \brief Define class JupyterMessageHash.
 **
 ** Takes Jupyter message data and generates a string hash,
 ** based on a given algorithm (currently only SHA256)
 ** for message authentication.
 */
class MessageHashGenerator
{
public:
  /** Default constructor */
  MessageHashGenerator();
  /** \brief Define the HMAC key and hash algorithm used for hash string generation.
  ** \param keyString HMAC digest string key.
  ** \param hashAlgorithm hash algorithm string identifier.
  ** \result true if arguments are valid (authentication is enabled). */
  bool SetKey( std::string keyString, std::string hashAlgorithm = "hmac-sha256" );
  
  /** \brief Generate hash string for Jupyter multipart message.
  ** \param header message part.
  ** \param parent message part.
  ** \param metadata message part.
  ** \param content message part. 
  ** \result generated hash string. */
  std::string GenerateHash( std::string& header, std::string& parent, std::string& metadata, std::string& content );
  
private:
  /** HMAC digest string key. */
  std::string key;
  /** OpenSSL HMAC digest engine (hash generator implementation). */
  const EVP_MD* digestEngine;
  /** Defines if authentication is enabled or not. */
  bool enabled;
};

#endif // MESSAGE_HASH_GENERATOR_HXX
