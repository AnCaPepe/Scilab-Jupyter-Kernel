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

#include "JupyterKernel.hxx"

#include "HistoryManager.h"
#include "configvariable.hxx"
#include "symbol.hxx"
#include "context.hxx"
#include "internal.hxx"
#include "parser.hxx"

extern "C" {
  #include "completion.h"
}

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include <chrono>

#ifndef _WIN32
  #include <unistd.h>
#else
  #include <windows.h>
#endif

const std::string JupyterKernel::USER_NAME = "kernel";

std::thread JupyterKernel::updateThread;

volatile bool JupyterKernel::isProcessing = false;
volatile bool JupyterKernel::isWaitingInput = false;
volatile bool JupyterKernel::isInputAllowed = true;

std::mutex JupyterKernel::inputLock;
std::string JupyterKernel::inputCurrentString;

std::mutex JupyterKernel::outputLock;
std::queue<std::string> JupyterKernel::outputQueue;

int JupyterKernel::Initialize( std::string connectionFileName ) 
{
  Json::Value connectionConfig;
  
  if( connectionFileName.empty() )
  {
    std::cerr << "Connection file name not provided" << std::endl;
    return -1;
  }
  
  // Load the JSON connection configuration file provided as the "{connection_file}"
  // command line argument listed in kernel.json spec file inside $JUPYTER_PATH/<kernel_name>/
  std::fstream connectionFile( connectionFileName, std::ios_base::in );
  // Use JsonCpp object stream operators to automatically parse and display the JSON file
  connectionFile >> connectionConfig;
  std::cout << connectionConfig << std::endl;
  
  // Make sure that Scilab engine is set to run continuosly
  ConfigVariable::setForceQuit( false );
  // Start the main update thread (most of messages detection and handling)
  updateThread = std::thread( &RunUpdateThread, connectionConfig );
  
  return 0;
}

void JupyterKernel::Terminate()
{
  isProcessing = false;                   // Make execution thread exit, if running
  ConfigVariable::setForceQuit( true );   // Make update thread exit
  updateThread.join();                    // Wait for update thread to exit
}

char* JupyterKernel::GetInputString()
{  
  inputLock.lock(); // Wait until there is input (command or requested from user) available
  
  char* inputString = (char*) inputCurrentString.data();
  
  return inputString;
}

void JupyterKernel::SetOutputString( const char* outputString )
{
  // Put received output at the end of the thread safe output queue
  outputLock.lock();
  outputQueue.push( outputString );
  outputLock.unlock();
}

void JupyterKernel::RunUpdateThread( Json::Value connectionConfig )
{
  // Get ZeroMQ based connection options from JsonCpp object data
  // Ideally, according to ZeroMQ docs, connection sockets should be created and used on the same thread
  JupyterKernelConnection connection( connectionConfig.get( "transport", "tcp" ).asString(), 
                                      connectionConfig.get( "ip", "*" ).asString(),
                                      connectionConfig.get( "iopub_port", "0" ).asString(),
                                      connectionConfig.get( "control_port", "0" ).asString(),
                                      connectionConfig.get( "stdin_port", "0" ).asString(),
                                      connectionConfig.get( "shell_port", "0" ).asString(),
                                      connectionConfig.get( "hb_port", "0" ).asString() );
  
  // Configure the message hash generator from the available JsonCpp object options
  connection.SetHashGenerator( connectionConfig.get( "key", "a0436f6c-1916-498b-8eb9-e81ab9368e84" ).asString(),
                               connectionConfig.get( "signature_scheme", "hmac-sha256" ).asString() );
  
  // Signal the start of the kernel/message handling to the running Jupyter clients
  JupyterMessage statusMessage( USER_NAME, "status" );
  statusMessage.content[ "execution_state" ] = "starting";
  connection.SendMessage( statusMessage );
  
  inputLock.lock();                           // We start without available input
  std::queue<JupyterMessage> messageInQueue;  // Messages are received in a priority queue
  while( not ConfigVariable::getForceQuit() ) // Run until other thread shuts Scilab engine down
  {
    connection.ReceiveMessages( messageInQueue );
    
    // If there is output available but no processing waiting for it, flush all to stdout stream
    if( not isProcessing ) FlushOutputStream( connection );
    
    while( not messageInQueue.empty() )
    {
      JupyterMessage& currentMessage = messageInQueue.front();
      
      std::string messageType = currentMessage.GetTypeString();

      if( messageType == "input_reply" ) HandleInputReply( currentMessage.content );
      else if( messageType == "execute_request" ) HandleExecutionRequest( connection, currentMessage );
      else if( messageType.find( "_request" ) )
      {
        // For every request message received kernel status should be updated before and after processing
        // "execute_request" is handled separately
        JupyterMessage statusMessage = currentMessage.GenerateReply( USER_NAME, "status" );
        statusMessage.content[ "execution_state" ] = "busy";
        connection.SendMessage( statusMessage );
        
        JupyterMessage replyMessage = currentMessage.GenerateReply( USER_NAME );
        
        if( messageType == "shutdown_request" ) HandleShutdownRequest( currentMessage.content, replyMessage.content );
        else if( messageType == "connect_request" ) FillConnectionInfo( connectionConfig, replyMessage.content );
        else if( messageType == "kernel_info_request" ) FillKernelInfo( replyMessage.content );
        else if( messageType == "history_request" ) HandleHistoryRequest( currentMessage.content, replyMessage.content );
        else if( messageType == "inspect_request" ) HandleInspectionRequest( currentMessage.content, replyMessage.content );
        else if( messageType == "complete_request" ) HandleCompletionRequest( currentMessage.content, replyMessage.content );
        else if( messageType == "is_complete_request" ) HandleCompletenessRequest( currentMessage, replyMessage.content );
        
        if( not replyMessage.content.empty() ) connection.SendMessage( replyMessage );
        
        statusMessage.content[ "execution_state" ] = "idle";
        connection.SendMessage( statusMessage );
      }
      
      messageInQueue.pop();
    }
  }
}

void JupyterKernel::FillKernelInfo( Json::Value& replyContent )
{            
  replyContent[ "language_info" ] = Json::Value( Json::objectValue );
  replyContent[ "language_info" ][ "name" ] = "scilab";
  replyContent[ "language_info" ][ "version" ] = "6.0";
  replyContent[ "language_info" ][ "mimetype" ] = "";
  replyContent[ "language_info" ][ "file_extension" ] = ".sce";
  replyContent[ "language_info" ][ "pygments_lexer" ] = "";
  replyContent[ "language_info" ][ "codemirror_mode" ] = "Octave";
  replyContent[ "language_info" ][ "nbconvert_exporter" ] = "";
  replyContent[ "protocol_version" ] = "5.0";
  replyContent[ "implementation" ] = "scilab";
  replyContent[ "implementation_version" ] = "0.1";
  replyContent[ "banner" ] = "Scilab Native Kernel (v0.1)";
}

void JupyterKernel::FillConnectionInfo( Json::Value& connectionConfig, Json::Value& replyContent )
{
  replyContent[ "shell_port" ] = connectionConfig.get( "shell_port", 0 ).asInt();
  replyContent[ "iopub_port" ] = connectionConfig.get( "iopub_port", 0 ).asInt();
  replyContent[ "stdin_port" ] = connectionConfig.get( "stdin_port", 0 ).asInt();
  replyContent[ "hb_port" ] = connectionConfig.get( "hb_port", 0 ).asInt();
}

void JupyterKernel::HandleHistoryRequest( Json::Value& commandContent, Json::Value& replyContent )
{
  // output and raw input options are currently not handled
  bool outputIncluded = commandContent.get( "output", false ).asBool();
  bool rawInputIncluded = commandContent.get( "raw", false ).asBool();
  
  int sessionNumber = commandContent.get( "session", 0 ).asInt();
  int startLine = commandContent.get( "start", 0 ).asInt();
  int stopLine = commandContent.get( "stop", 0 ).asInt();
  
  setSearchedTokenInScilabHistory( NULL );  // Adds all history to the search
  int historyLength = getSizeAllLinesOfScilabHistory();
  
  std::string accessType = commandContent.get( "hist_access_type", "range" ).asString();
  if( accessType == "range" )
  {    
    // Preventing out of bounds request range
    if( stopLine >= historyLength ) stopLine = historyLength - 1;
    if( startLine > stopLine ) startLine = stopLine;
  }
  else
  {
    stopLine = historyLength - 1;
    
    if( accessType == "tail" )
    {
      // There is no way to define search interval, so this value is only used for "tail" option
      int cellsNumber = commandContent.get( "n", 0 ).asInt();
      startLine = historyLength - cellsNumber;
    }
    else if( accessType == "search" )
    {
      std::string searchToken = commandContent.get( "pattern", "" ).asString();
      bool isUnique = commandContent.get( "unique", false ).asBool();
      
      // Define search token (updates search output list)
      setSearchedTokenInScilabHistory( (char*) searchToken.data() );
      int historySearchLength = getSizeAllLinesOfScilabHistory();
      
      // Allow only the last match to be sent to client
      if( isUnique && historySearchLength > 1 ) historySearchLength = 1; 
      startLine = historyLength - historySearchLength;
    }
  }
  
  replyContent[ "history" ] = Json::Value( Json::arrayValue );
  // History range/search entries are browsed from the last to the first
  for( int historyLineIndex = stopLine; historyLineIndex >= startLine; historyLineIndex-- )
  {
    // It looks like session number is different than session UUID (it's an integer
    // rather than a string). So I'm just assuming it is right and returning the same value
    std::stringstream historyLineStream( "(" );
    historyLineStream << sessionNumber;
    historyLineStream << ",";
    historyLineStream << historyLineIndex;
    historyLineStream << "," << getPreviousLineInScilabHistory() << ")";
    
    replyContent[ "history" ][ historyLineIndex ] = historyLineStream.str();
  }
}

void JupyterKernel::HandleInspectionRequest( Json::Value& commandContent, Json::Value& replyContent )
{
  std::string code = commandContent.get( "code", "" ).asString();
  int cursorPosition = commandContent.get( "cursor_pos", 0 ).asInt();
  
  // Not differentiating between levels of detail so far
  int detailLevel = commandContent.get( "detail_level", 0 ).asInt();
  
  // Consider the searched variable name/symbol the word (between spaces) over which the cursor is located
  size_t variableNameStartPosition = code.substr( 0, cursorPosition ).find_last_of( " \n\r\t" );
  if( variableNameStartPosition == std::string::npos ) variableNameStartPosition = 0;
  else variableNameStartPosition += 1;
  
  size_t variableNameEndPosition = code.substr( cursorPosition ).find_first_of( " \n\r\t" );
  
  std::string variableName = code.substr( variableNameStartPosition, variableNameEndPosition );
  
  // A symbol is defined by a wide string (more than 8 bits per character), so we perform a format conversion
  symbol::Symbol variableSymbol( std::wstring( variableName.begin(), variableName.end() ) );
  // Scilab's context object is a single instance (singleton) shared accross the entire aplication
  types::InternalType* variableType = symbol::Context::getInstance()->get( variableSymbol );
  
  replyContent[ "status" ] = "ok";
  replyContent[ "found" ] = false;
  replyContent[ "data" ] = Json::Value( Json::objectValue );
  replyContent[ "metadata" ] = Json::Value( Json::objectValue );
  
  if( variableType != NULL ) 
  {
    replyContent[ "found" ] = true;
    // For now, we only acquire the variable type string identifier
    std::wstring typeWString = variableType->getTypeStr();
    replyContent[ "data" ][ "Type" ] = std::string( typeWString.begin(), typeWString.end() );
  }
}

void JupyterKernel::HandleCompletionRequest( Json::Value& commandContent, Json::Value& replyContent )
{
  std::string code = commandContent.get( "code", "" ).asString();
  int cursorPosition = commandContent.get( "cursor_pos", 0 ).asInt();
  
  // Consider the searched token only the substring of the code 
  // after the last space character and before the cursor position
  code.resize( cursorPosition, ' ' );
  size_t tokenStartPosition = code.find_last_of( " \n\r\t" );
  if( tokenStartPosition == std::string::npos ) tokenStartPosition = 0;
  else tokenStartPosition += 1;
  std::string token = code.substr( tokenStartPosition );
  
  // Get the number and list of matching strings
  int completionMatchesCount = 0;
  char** completionMatches = completion( (const char*) token.data(), &completionMatchesCount );
  
  // Create and fill the returning dictionary
  replyContent[ "matches" ] = Json::Value( Json::arrayValue );
  for( int completionMatchIndex = 0; completionMatchIndex < completionMatchesCount; completionMatchIndex++ )
    replyContent[ "matches" ][ completionMatchIndex ] = std::string( completionMatches[ completionMatchIndex ] );
  
  // I guess it should return "ok" even with no matches found
  replyContent[ "cursor_start" ] = (int) tokenStartPosition;
  replyContent[ "cursor_end" ] = cursorPosition;
  replyContent[ "metadata" ] = Json::Value( Json::objectValue );
  replyContent[ "status" ] = "ok";
}

void JupyterKernel::HandleCompletenessRequest( JupyterMessage& commandMessage, Json::Value& replyContent )
{
  // Independent parser (not the engine's one) "static" so that previous state is kept
  // TODO: Should become a std::map (mapped by identifier) someday to account for multiple clients
  static std::map<std::string, Parser> checkersTable;
  
  std::string identifier = commandMessage.identifier;
  std::string code = commandMessage.content.get( "code", "" ).asString();
  
  // Thankfully, std::map [] operator already inserts a new element if it doesn't exist
  Parser& checker = checkersTable[ identifier ];
  
  checker.parse( code.data() );     // Verify code without submitting it to the engine
  
  bool isComplete = false;
  replyContent[ "status" ] = "incomplete"; // when "unknown" ?
  if( checker.getControlStatus() == Parser::ControlStatus::AllControlClosed )
  {
    isComplete = true;
    if( checker.getExitStatus() == Parser::ParserStatus::Succeded )
      replyContent[ "status" ] = "complete";
    else
    {
      replyContent[ "status" ] = "invalid";
      checker.cleanup();
    }
  }
  
  // Frontends like QtConsole use their own prompt string and 
  // appends the received one to the code, so this should be empty
  if( not isComplete ) replyContent[ "indent" ] = "";
}

void JupyterKernel::HandleExecutionRequest( JupyterKernelConnection& publisher, JupyterMessage& commandMessage )
{
  static unsigned int executionCount;
  
  JupyterMessage statusMessage = commandMessage.GenerateReply( USER_NAME, "status" );
  statusMessage.content[ "execution_state" ] = "busy";
  publisher.SendMessage( statusMessage );
  
  Json::Value& commandContent = commandMessage.content;
  bool silent = commandContent.get( "silent", false ).asBool();
  bool storeHistory = commandContent.get( "store_history", not silent ).asBool();
  std::string commandString = commandContent.get( "code", "" ).asString();
  
  // Some clients don't allow input requests and can inform it
  isInputAllowed = commandContent.get( "allow_stdin", true ).asBool();
  
  if( not silent )  // Results are not published if client requests silent execution
  {
    JupyterMessage inputMessage = commandMessage.GenerateReply( USER_NAME, "execute_input" );
    inputMessage.content[ "execution_count" ] = executionCount;
    inputMessage.content[ "code" ] = commandString;
    publisher.SendMessage( inputMessage );
  }
  
  if( storeHistory )  // History is only updated if required
  {
    appendLineToScilabHistory( (char*) commandString.data() );
    executionCount++;
  }
  
  // We don't need to store a handle for this thread, as a 
  // detached thread doesn't need to be awaited (joined) for returning
  std::thread( &HandleExecutionReply, std::ref( publisher ), commandMessage ).detach();
  
  // Set the new command input string and unlock its reading by the internal engine
  inputCurrentString = commandString;
  inputLock.unlock();
  isProcessing = true;
}

void JupyterKernel::HandleExecutionReply( JupyterKernelConnection& publisher, JupyterMessage commandMessage )
{
  static unsigned int executionCount;
  
  Json::Value& commandContent = commandMessage.content;
  bool silent = commandContent.get( "silent", false ).asBool();
  bool storeHistory = commandContent.get( "store_history", not silent ).asBool();
  std::string shellIdentity = commandMessage.identifier;
  
  JupyterMessage resultMessage = commandMessage.GenerateReply( USER_NAME, "execute_result" );
  
  while( isProcessing )
  {
    // Kinda hacky. Give time for the core Scilab threads to process
    std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
    
    // We're waiting form input. Request it from the client
    if( not ConfigVariable::isScilabCommand() ) HandleInputRequest( publisher, shellIdentity );
    // Run until the command prompt is available again
    else if( ConfigVariable::getPromptMode() == 2 ) break;
  }
  
  std::string resultString = GetCurrentOutput();
  
  // TODO: handle more complex types of execution result publishing
  if( not silent ) 
  {
    resultMessage.content[ "execution_count" ] = executionCount;
    resultMessage.content[ "data" ] = Json::Value( Json::objectValue );
    resultMessage.content[ "data" ][ "text/plain" ] = resultString;
    resultMessage.content[ "metadata" ] = Json::Value( Json::objectValue );
    publisher.SendMessage( resultMessage );
  }
  
  std::cout << "Execution Completed" << std::endl;
  
  JupyterMessage replyMessage = commandMessage.GenerateReply( USER_NAME );
  
  // Very basic reply and error management
  // TODO: Properly implement user user_expressions
  // TODO: Implement payloads
  if( ConfigVariable::isError() )
  {
    replyMessage.content[ "status" ] = "error";
    std::wstring& errorString = ConfigVariable::getLastErrorMessage();
    replyMessage.content[ "ename" ] = std::string( errorString.begin(), errorString.end() );
    replyMessage.content[ "evalue" ] = std::to_string( ConfigVariable::getLastErrorNumber() );
    std::wstring traceString = ConfigVariable::getLastErrorFunction() + L":" + std::to_wstring( ConfigVariable::getLastErrorLine() );
    replyMessage.content[ "traceback" ] = std::string( traceString.begin(), traceString.end() );
  }
  else
  {
    Json::Value userExpressions = commandContent.get( "user_expressions", Json::Value( Json::objectValue ) );
    for( size_t exprIndex = 0; exprIndex < userExpressions.size(); exprIndex++ )
      std::cout << userExpressions.get( exprIndex, "<null>" ).asString();
    std::cout << std::endl;
    
    replyMessage.content[ "status" ] = "ok";
    replyMessage.content[ "execution_count" ] = executionCount;
    replyMessage.content[ "payload" ] = Json::Value( Json::arrayValue );
    replyMessage.content[ "user_expressions" ] = userExpressions;
  }
  publisher.SendMessage( replyMessage );
  
  if( storeHistory ) 
  {
    // Someday... There is currently no way to differentiate input from output in History Manager
    //appendLineToScilabHistory( (char*) resultString.data() );
    executionCount++;
  }
  
  JupyterMessage statusMessage = commandMessage.GenerateReply( USER_NAME, "status" );
  statusMessage.content[ "execution_state" ] = "idle";
  publisher.SendMessage( statusMessage );
  
  isProcessing = false;
}

void JupyterKernel::HandleInputRequest( JupyterKernelConnection& requester, std::string shellIdentity )
{
  // Prevents it from being called multiple times for the same mscanf
  if( isWaitingInput ) return;
  
  if( not isInputAllowed )
  {
    ConfigVariable::setConsoleReadStr( (char*) "" );
    return;
  }
  
  isWaitingInput = true;
    
  // Use all the previous results as the prompt string, to make it appear in order
  std::string promptString = GetCurrentOutput();
    
  JupyterMessage inputRequestMessage( USER_NAME, "input_request" );
    
  // Input requests have to use the same identity as the client shell
  inputRequestMessage.identifier = shellIdentity;
  inputRequestMessage.content[ "prompt" ] = promptString;
  inputRequestMessage.content[ "password" ] = false;
    
  requester.SendMessage( inputRequestMessage );
}

void JupyterKernel::HandleInputReply( Json::Value& replyContent )
{
  if( not isWaitingInput ) return;
  
  std::string stdinString = replyContent.get( "value", "" ).asString();

  isWaitingInput = false;
  
  //ConfigVariable::setConsoleReadStr( (char*) stdinString.data() );
  inputCurrentString = stdinString;
  inputLock.unlock();
}

void JupyterKernel::HandleShutdownRequest( Json::Value& commandContent, Json::Value& replyContent )
{
  std::cout << "Shutting Scilab kernel down" << std::endl;
  
  replyContent[ "restart" ] = commandContent.get( "restart", false ).asBool();
          
  // If client doesn't request restart, stop Scilab engine and Jupyter update thread
  // TODO: Handle restart=true option properly (Clean context ?)
  if( not replyContent[ "restart" ] ) ConfigVariable::setForceQuit( true );
}

void JupyterKernel::FlushOutputStream( JupyterKernelConnection& publisher )
{
  JupyterMessage streamMessage( USER_NAME, "stream" );
  
  std::string stdoutString = GetCurrentOutput();
  
  streamMessage.content[ "name" ] = "stdout";
  streamMessage.content[ "text" ] = stdoutString;
    
  publisher.SendMessage( streamMessage );
  
  outputLock.unlock();
}

std::string JupyterKernel::GetCurrentOutput()
{
  std::string outputString = "";
  
  // Take and concatenate all strings from the output queue
  outputLock.lock();
  while( not outputQueue.empty() )
  {
    outputString += outputQueue.front();
    outputQueue.pop();
  }
  outputLock.unlock();
  
  return outputString;
}
