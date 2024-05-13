package app.organicmaps.sdk.location;

import android.annotation.SuppressLint;
import android.net.SSLCertificateSocketFactory;
import android.os.SystemClock;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import app.organicmaps.BuildConfig;
import app.organicmaps.util.log.Logger;

import javax.net.SocketFactory;
import javax.net.ssl.SSLSocketFactory;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.Socket;
import java.net.SocketException;
import java.net.SocketTimeoutException;

/**
 * Implements interface that will be used by the core for
 * sending/receiving the raw data trough platform socket interface.
 * <p>
 * The instance of this class is supposed to be created in JNI layer
 * and supposed to be used in the thread safe environment, i.e. thread safety
 * should be provided externally (by the client of this class).
 * <p>
 * <b>All public methods are blocking and shouldn't be called from the main thread.</b>
 */
// Called from JNI.
@Keep
@SuppressWarnings("unused")
class PlatformSocket
{
  private static final String TAG = PlatformSocket.class.getSimpleName();

  private final static int DEFAULT_TIMEOUT = 30 * 1000;
  private static volatile long sSslConnectionCounter;
  @Nullable
  private Socket mSocket;
  @Nullable
  private String mHost;
  private int mPort;
  private int mTimeout = DEFAULT_TIMEOUT;

  PlatformSocket()
  {
    sSslConnectionCounter = 0;
    Logger.d(TAG, "***********************************************************************************");
    Logger.d(TAG, "Platform socket is created by core, ssl connection counter is discarded.");
  }

  public boolean open(@NonNull String host, int port)
  {
    if (mSocket != null)
    {
      Logger.e(TAG, "Socket is already opened. Seems that it wasn't closed.");
      return false;
    }

    if (!isPortAllowed(port))
    {
      Logger.e(TAG, "A wrong port number = " + port + ", it must be within (0-65535) range");
      return false;
    }

    mHost = host;
    mPort = port;

    Socket socket = createSocket(host, port, true);
    if (socket != null && socket.isConnected())
    {
      setReadSocketTimeout(socket, mTimeout);
      mSocket = socket;
    }

    return mSocket != null;
  }

  private static boolean isPortAllowed(int port)
  {
    return port >= 0 && port <= 65535;
  }

  @Nullable
  private static Socket createSocket(@NonNull String host, int port, boolean ssl)
  {
    return ssl ? createSslSocket(host, port) : createRegularSocket(host, port);
  }

  @Nullable
  private static Socket createSslSocket(@NonNull String host, int port)
  {
    Socket socket = null;
    try
    {
      SocketFactory sf = getSocketFactory();
      socket = sf.createSocket(host, port);
      sSslConnectionCounter++;
      Logger.d(TAG, "###############################################################################");
      Logger.d(TAG, sSslConnectionCounter + " ssl connection is established.");
    }
    catch (IOException e)
    {
      Logger.e(TAG, "Failed to create the ssl socket, mHost = " + host + " mPort = " + port);
    }
    return socket;
  }

  @Nullable
  private static Socket createRegularSocket(@NonNull String host, int port)
  {
    Socket socket = null;
    try
    {
      socket = new Socket(host, port);
      Logger.d(TAG, "Regular socket is created and tcp handshake is passed successfully");
    }
    catch (IOException e)
    {
      Logger.e(TAG, "Failed to create the socket, mHost = " + host + " mPort = " + port);
    }
    return socket;
  }

  @SuppressLint("SSLCertificateSocketFactoryGetInsecure")
  @NonNull
  private static SocketFactory getSocketFactory()
  {
    // Trusting to any ssl certificate factory that will be used in
    // debug mode, for testing purposes only.
    if (BuildConfig.DEBUG)
      //TODO: implement the custom KeyStore to make the self-signed certificates work
      return SSLCertificateSocketFactory.getInsecure(0, null);

    return SSLSocketFactory.getDefault();
  }

  public void close()
  {
    if (mSocket == null)
    {
      Logger.d(TAG, "Socket is already closed or it wasn't opened yet\n");
      return;
    }

    try
    {
      mSocket.close();
      Logger.d(TAG, "Socket has been closed: " + this + "\n");
    } catch (IOException e)
    {
      Logger.e(TAG, "Failed to close socket: " + this + "\n");
    } finally
    {
      mSocket = null;
    }
  }

  public boolean read(@NonNull byte[] data, int count)
  {
    if (!checkSocketAndArguments(data, count))
      return false;

    Logger.d(TAG, "Reading method is started, data.length = " + data.length + ", count = " + count);
    long startTime = SystemClock.elapsedRealtime();
    int readBytes = 0;
    try
    {
      if (mSocket == null)
        throw new AssertionError("mSocket cannot be null");

      InputStream in = mSocket.getInputStream();
      while (readBytes != count && (SystemClock.elapsedRealtime() - startTime) < mTimeout)
      {
        try
        {
          Logger.d(TAG, "Attempting to read " + count + " bytes from offset = " + readBytes);
          int read = in.read(data, readBytes, count - readBytes);

          if (read == -1)
          {
            Logger.d(TAG, "All data is read from the stream, read bytes count = " + readBytes + "\n");
            break;
          }

          if (read == 0)
          {
            Logger.e(TAG, "0 bytes are obtained. It's considered as error\n");
            break;
          }

          Logger.d(TAG, "Read bytes count = " + read + "\n");
          readBytes += read;
        } catch (SocketTimeoutException e)
        {
          long readingTime = SystemClock.elapsedRealtime() - startTime;
          Logger.e(TAG, "Socked timeout has occurred after " + readingTime + " (ms)\n ");
          if (readingTime > mTimeout)
          {
            Logger.e(TAG, "Socket wrapper timeout has occurred, requested count = " +
                     (count - readBytes) + ", readBytes = " + readBytes + "\n");
            break;
          }
        }
      }
    } catch (IOException e)
    {
      Logger.e(TAG, "Failed to read data from socket: " + this + "\n");
    }

    return count == readBytes;
  }

  public boolean write(@NonNull byte[] data, int count)
  {
    if (!checkSocketAndArguments(data, count))
      return false;

    Logger.d(TAG, "Writing method is started, data.length = " + data.length + ", count = " + count);
    long startTime = SystemClock.elapsedRealtime();
    try
    {
      if (mSocket == null)
        throw new AssertionError("mSocket cannot be null");

      OutputStream out = mSocket.getOutputStream();
      out.write(data, 0, count);
      Logger.d(TAG, count + " bytes are written\n");
      return true;
    } catch (SocketTimeoutException e)
    {
      long writingTime = SystemClock.elapsedRealtime() - startTime;
      Logger.e(TAG, "Socked timeout has occurred after " + writingTime + " (ms)\n");
    } catch (IOException e)
    {
      Logger.e(TAG, "Failed to write data to socket: " + this + "\n");
    }

    return false;
  }

  private boolean checkSocketAndArguments(@NonNull byte[] data, int count)
  {
    if (mSocket == null)
    {
      Logger.e(TAG, "Socket must be opened before reading/writing\n");
      return false;
    }

    if (count < 0 || count > data.length)
    {
      Logger.e(TAG, "Illegal arguments, data.length = " + data.length + ", count = " + count + "\n");
      return false;
    }

    return true;
  }

  public void setTimeout(int millis)
  {
    mTimeout = millis;
    Logger.d(TAG, "Setting the socket wrapper timeout = " + millis + " ms\n");
  }

  private void setReadSocketTimeout(@NonNull Socket socket, int millis)
  {
    try
    {
      socket.setSoTimeout(millis);
    } catch (SocketException e)
    {
      Logger.e(TAG, "Failed to set system socket timeout: " + millis + "ms, " + this + "\n");
    }
  }

  @Override
  public String toString()
  {
    return "PlatformSocket{" +
           "mSocket=" + mSocket +
           ", mHost='" + mHost + '\'' +
           ", mPort=" + mPort +
           '}';
  }
}
