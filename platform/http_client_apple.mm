/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Alexander Borsuk <me@alex.bio> from Minsk, Belarus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#if ! __has_feature(objc_arc)
#error This file must be compiled with ARC. Either turn on ARC for the project or use -fobjc-arc flag
#endif

#import <Foundation/Foundation.h>

#include "platform/http_client.hpp"
#import "platform/http_session_manager.h"

#include "base/logging.hpp"


@interface RedirectDelegate : NSObject<NSURLSessionDataDelegate>

// If YES - redirect response triggeres new request automatically
// If NO  - redirect response is returned to result handler
@property(nonatomic) BOOL followRedirects;

- (instancetype)init:(BOOL)followRedirects;

- (void)        URLSession:(NSURLSession *)session
                      task:(NSURLSessionTask *)task
willPerformHTTPRedirection:(NSHTTPURLResponse *)response
                newRequest:(NSURLRequest *)newRequest
         completionHandler:(void (^)(NSURLRequest *))completionHandler;
@end

@implementation RedirectDelegate
- (instancetype)init:(BOOL)followRedirects
{
  if (self = [super init])
    _followRedirects = followRedirects;
  
  return self;
}

- (void)        URLSession:(NSURLSession *)session
                      task:(NSURLSessionTask *)task
willPerformHTTPRedirection:(NSHTTPURLResponse *)response
                newRequest:(NSURLRequest *)newRequest
         completionHandler:(void (^)(NSURLRequest *))completionHandler
{
  if (!_followRedirects && response.statusCode >= 300 && response.statusCode < 400)
    completionHandler(nil);
  else
    completionHandler(newRequest);
}
@end


@interface Connection : NSObject
+ (nullable NSData *)sendSynchronousRequest:(NSURLRequest *)request
                            followRedirects:(BOOL)followRedirects
                          returningResponse:(NSURLResponse **)response
                                      error:(NSError **)error;
@end

@implementation Connection

+ (NSData *)sendSynchronousRequest:(NSURLRequest *)request
                   followRedirects:(BOOL)followRedirects
                 returningResponse:(NSURLResponse * __autoreleasing *)response
                             error:(NSError * __autoreleasing *)error
{
  Connection * connection = [[Connection alloc] init];
  return [connection sendSynchronousRequest:request followRedirects: followRedirects returningResponse:response error:error];
}

- (NSData *)sendSynchronousRequest:(NSURLRequest *)request
                   followRedirects:(BOOL)followRedirects
                 returningResponse:(NSURLResponse * __autoreleasing *)response
                             error:(NSError * __autoreleasing *)error {
  __block NSData * resultData = nil;
  __block NSURLResponse * resultResponse = nil;
  __block NSError * resultError = nil;

  dispatch_group_t group = dispatch_group_create();
  dispatch_group_enter(group);

  RedirectDelegate * delegate = [[RedirectDelegate alloc] init: followRedirects];

  [[[HttpSessionManager sharedManager]
      dataTaskWithRequest:request
                 delegate:delegate
        completionHandler:^(NSData * _Nullable data, NSURLResponse * _Nullable response,
                            NSError * _Nullable error) {
          resultData = data;
          resultResponse = response;
          resultError = error;
          dispatch_group_leave(group);
        }] resume];

  dispatch_group_wait(group, DISPATCH_TIME_FOREVER);
  *response = resultResponse;
  *error = resultError;
  return resultData;
}
@end

namespace platform
{
bool HttpClient::RunHttpRequest()
{
  NSURL * url = static_cast<NSURL *>([NSURL URLWithString:@(m_urlRequested.c_str())]);
  NSMutableURLRequest * request = [NSMutableURLRequest requestWithURL: url
      cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:m_timeoutSec];
  // We handle cookies manually.
  request.HTTPShouldHandleCookies = NO;

  request.HTTPMethod = @(m_httpMethod.c_str());
  for (auto const & header : m_headers)
  {
    NSString * field = @(header.first.c_str());
    [request setValue:@(header.second.c_str()) forHTTPHeaderField:field];
  }

  if (!m_cookies.empty())
    [request setValue:[NSString stringWithUTF8String:m_cookies.c_str()] forHTTPHeaderField:@"Cookie"];

  if (!m_bodyData.empty())
  {
    request.HTTPBody = [NSData dataWithBytes:m_bodyData.data() length:m_bodyData.size()];
    LOG(LDEBUG, ("Uploading buffer of size", m_bodyData.size(), "bytes"));
  }
  else if (!m_inputFile.empty())
  {
    NSError * err = nil;
    NSString * path = [NSString stringWithUTF8String:m_inputFile.c_str()];
    const unsigned long long file_size = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&err].fileSize;
    if (err)
    {
      m_errorCode = static_cast<int>(err.code);
      LOG(LDEBUG, ("Error: ", m_errorCode, err.localizedDescription.UTF8String));

      return false;
    }
    request.HTTPBodyStream = [NSInputStream inputStreamWithFileAtPath:path];
    [request setValue:[NSString stringWithFormat:@"%llu", file_size] forHTTPHeaderField:@"Content-Length"];
    LOG(LDEBUG, ("Uploading file", m_inputFile, file_size, "bytes"));
  }

  NSHTTPURLResponse * response = nil;
  NSError * err = nil;
  NSData * url_data = [Connection sendSynchronousRequest:request followRedirects:m_followRedirects returningResponse:&response error:&err];

  m_headers.clear();

  if (response)
  {
    m_errorCode = static_cast<int>(response.statusCode);

    NSString * redirectUri = [response.allHeaderFields objectForKey:@"Location"];
    if (redirectUri)
      m_urlReceived = redirectUri.UTF8String;
    else
      m_urlReceived = response.URL.absoluteString.UTF8String;

    if (m_loadHeaders)
    {
      [response.allHeaderFields enumerateKeysAndObjectsUsingBlock:^(NSString * key, NSString * obj, BOOL *)
      {
        m_headers.emplace(key.lowercaseString.UTF8String, obj.UTF8String);
      }];
    }
    else
    {
      NSString * cookies = [response.allHeaderFields objectForKey:@"Set-Cookie"];
      if (cookies)
        m_headers.emplace("Set-Cookie", NormalizeServerCookies(cookies.UTF8String));
    }

    if (url_data)
    {
      if (m_outputFile.empty())
        m_serverResponse.assign(reinterpret_cast<char const *>(url_data.bytes), url_data.length);
      else
        [url_data writeToFile:@(m_outputFile.c_str()) atomically:YES];

    }

    return true;
  }
  // Request has failed if we are here.
  // MacOSX/iOS-specific workaround for HTTP 401 error bug.
  // @see bit.ly/1TrHlcS for more details.
  if (err.code == NSURLErrorUserCancelledAuthentication)
  {
    m_errorCode = 401;
    return true;
  }

  m_errorCode = static_cast<int>(err.code);
  LOG(LDEBUG, ("Error: ", m_errorCode, ':', err.localizedDescription.UTF8String,
               "while connecting to", m_urlRequested));

  return false;
}
} // namespace platform
