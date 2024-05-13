/*******************************************************************************
 * The MIT License (MIT)
 * <p/>
 * Copyright (c) 2014 Alexander Borsuk <me@alex.bio> from Minsk, Belarus
 * <p/>
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * <p/>
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * <p/>
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *******************************************************************************/

package app.organicmaps.sdk.util;

import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import app.organicmaps.util.log.Logger;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;
import java.util.zip.GZIPInputStream;
import java.util.zip.InflaterInputStream;

// Used by JNI.
@Keep
@SuppressWarnings("unused")
public final class HttpClient
{
  private static final String TAG = HttpClient.class.getSimpleName();

  // TODO(AlexZ): tune for larger files
  private final static int STREAM_BUFFER_SIZE = 1024 * 64;

  public static Params run(@NonNull final Params p) throws IOException, NullPointerException
  {
    if (TextUtils.isEmpty(p.httpMethod))
      throw new IllegalArgumentException("Please set valid HTTP method for request at Params.httpMethod field.");

    HttpURLConnection connection = null;

    Logger.d(TAG, "Connecting to " + Utils.makeUrlSafe(p.url));

    try
    {
      connection = (HttpURLConnection) new URL(p.url).openConnection();

      // NullPointerException, MalformedUrlException, IOException
      // Redirects from http to https or vice versa are not supported by Android implementation.
      // There is also a nasty bug on Androids before 4.4:
      // if you send any request with Content-Length set, and it is redirected, and your instance is set to automatically follow redirects,
      // then next (internal) GET request to redirected location will incorrectly have have all headers set from the previous request,
      // including Content-Length, Content-Type etc. This leads to unexpected hangs and timeout errors, because some servers are
      // correctly trying to wait for the body if Content-Length is set.
      // It shows in logs like this:
      //
      // java.net.SocketTimeoutException: Read timed out
      //   at org.apache.harmony.xnet.provider.jsse.NativeCrypto.SSL_read(Native Method)
      //   at org.apache.harmony.xnet.provider.jsse.OpenSSLSocketImpl$SSLInputStream.read(OpenSSLSocketImpl.java:687)
      //   ...
      //
      // Looks like this bug was fixed by switching to OkHttp implementation in this commit:
      // https://android.googlesource.com/platform/libcore/+/2503556d17b28c7b4e6e514540a77df1627857d0
      connection.setInstanceFollowRedirects(p.followRedirects);
      connection.setConnectTimeout(p.timeoutMillisec);
      connection.setReadTimeout(p.timeoutMillisec);
      connection.setUseCaches(false);
      connection.setRequestMethod(p.httpMethod);

      if (!TextUtils.isEmpty(p.cookies))
        connection.setRequestProperty("Cookie", p.cookies);

      for (KeyValue header : p.headers)
        connection.setRequestProperty(header.getKey(), header.getValue());

      if (!TextUtils.isEmpty(p.inputFilePath) || p.data != null)
      {
        // Send (POST, PUT...) data to the server.
        if (TextUtils.isEmpty(connection.getRequestProperty("Content-Type")))
          throw new NullPointerException("Please set Content-Type for request.");

        // Work-around for situation when more than one consequent POST requests can lead to stable
        // "java.net.ProtocolException: Unexpected status line:" on a client and Nginx HTTP 499 errors.
        // The only found reference to this bug is http://stackoverflow.com/a/24303115/1209392
        connection.setRequestProperty("Connection", "close");
        connection.setDoOutput(true);
        if (p.data != null)
        {
          connection.setFixedLengthStreamingMode(p.data.length);
          try (OutputStream os = connection.getOutputStream())
          {
            os.write(p.data);
          }
          Logger.d(TAG, "Sent " + p.httpMethod + " with content of size " + p.data.length);
        }
        else
        {
          final File file = new File(p.inputFilePath);
          connection.setFixedLengthStreamingMode((int) file.length());
          final BufferedInputStream istream = new BufferedInputStream(new FileInputStream(file), STREAM_BUFFER_SIZE);
          final BufferedOutputStream ostream = new BufferedOutputStream(connection.getOutputStream(), STREAM_BUFFER_SIZE);
          final byte[] buffer = new byte[STREAM_BUFFER_SIZE];
          int bytesRead;
          while ((bytesRead = istream.read(buffer, 0, STREAM_BUFFER_SIZE)) > 0)
          {
            ostream.write(buffer, 0, bytesRead);
          }
          istream.close(); // IOException
          ostream.close(); // IOException
          Logger.d(TAG, "Sent " + p.httpMethod + " with file of size " + file.length());
        }
      }
      // GET data from the server or receive response body
      p.httpResponseCode = connection.getResponseCode();
      Logger.d(TAG, "Received HTTP " + p.httpResponseCode + " from server, content encoding = " +
               connection.getContentEncoding() + ", for request = " + Utils.makeUrlSafe(p.url));

      if (p.httpResponseCode >= 300 && p.httpResponseCode < 400)
        p.receivedUrl = connection.getHeaderField("Location");
      else
        p.receivedUrl = connection.getURL().toString();

      p.headers.clear();
      if (p.loadHeaders)
      {
        for (Map.Entry<String, List<String>> header : connection.getHeaderFields().entrySet())
        {
          // Some implementations include a mapping for the null key.
          if (header.getKey() == null || header.getValue() == null)
            continue;

          p.headers.add(new KeyValue(StringUtils.toLowerCase(header.getKey()), TextUtils.join(", ",
              header.getValue())));
        }
      }
      else
      {
        List<String> cookies = connection.getHeaderFields().get("Set-Cookie");
        if (cookies != null)
          p.headers.add(new KeyValue("Set-Cookie", TextUtils.join(", ", cookies)));
      }

      try
      {
        OutputStream ostream;
        if (!TextUtils.isEmpty(p.outputFilePath))
          ostream = new BufferedOutputStream(new FileOutputStream(p.outputFilePath), STREAM_BUFFER_SIZE);
        else
          ostream = new ByteArrayOutputStream(STREAM_BUFFER_SIZE);
        // TODO(AlexZ): Add HTTP resume support in the future for partially downloaded files
        final BufferedInputStream istream = new BufferedInputStream(getInputStream(connection), STREAM_BUFFER_SIZE);
        final byte[] buffer = new byte[STREAM_BUFFER_SIZE];
        // gzip encoding is transparently enabled and we can't use Content-Length for
        // body reading if server has gzipped it.
        int bytesRead;
        while ((bytesRead = istream.read(buffer, 0, STREAM_BUFFER_SIZE)) > 0) {
          // Read everything if Content-Length is not known in advance.
          ostream.write(buffer, 0, bytesRead);
        }
        istream.close(); // IOException
        ostream.close(); // IOException
        if (ostream instanceof ByteArrayOutputStream)
          p.data = ((ByteArrayOutputStream) ostream).toByteArray();
      }
      catch (IOException ex)
      {
        // Exception here means that there is no body in the response.
      }
    }
    finally
    {
      if (connection != null)
        connection.disconnect();
    }
    return p;
  }

  @NonNull
  private static InputStream getInputStream(@NonNull HttpURLConnection connection) throws IOException
  {
    InputStream in;
    try
    {
      if ("gzip".equals(connection.getContentEncoding()))
        in = new GZIPInputStream(connection.getInputStream());
      else if ("deflate".equals(connection.getContentEncoding()))
        in = new InflaterInputStream(connection.getInputStream());
      else
        in = connection.getInputStream();
    }
    catch (IOException e)
    {
      in = connection.getErrorStream();
      if (in == null)
        throw e;
    }
    return in;
  }

  // Used by JNI.
  @Keep
  @SuppressWarnings("unused")
  private static class Params
  {
    public void setHeaders(@NonNull KeyValue[] array)
    {
      headers = new ArrayList<>(Arrays.asList(array));
    }

    public Object[] getHeaders()
    {
      return headers.toArray();
    }

    public String url;
    // Can be different from url in case of redirects.
    String receivedUrl;
    String httpMethod;
    // Should be specified for any request whose method allows non-empty body.
    // On return, contains received Content-Type or null.
    // Can be specified for any request whose method allows non-empty body.
    // On return, contains received Content-Encoding or null.
    public byte[] data;
    // Send from input file if specified instead of data.
    String inputFilePath;
    // Received data is stored here if not null or in data otherwise.
    String outputFilePath;
    String cookies;
    ArrayList<KeyValue> headers = new ArrayList<>();
    int httpResponseCode = -1;
    boolean followRedirects = true;
    boolean loadHeaders;
    int timeoutMillisec = Constants.READ_TIMEOUT_MS;

    // Simple GET request constructor.
    public Params(String url)
    {
      this.url = url;
      httpMethod = "GET";
    }
  }
}
