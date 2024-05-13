package app.organicmaps.sdk.util;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.security.KeyStore;
import java.security.SecureRandom;

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.SSLSocketFactory;

public class ClientCertTLSSocketFactory
{
  private static final String PROTOCOL = "TLS";
  private static final String ALGORITHM = "X509";
  private static final String KEY_STORE_TYPE = "PKCS12";

  @NonNull
  public static SSLSocketFactory create(@NonNull byte[] payload, @Nullable char[] password)
  {
    InputStream inputStream = null;
    try
    {
      inputStream = new ByteArrayInputStream(payload);
      KeyStore keyStore = KeyStore.getInstance(KEY_STORE_TYPE);
      keyStore.load(inputStream, password);
      KeyManagerFactory kmf = KeyManagerFactory.getInstance(ALGORITHM);
      kmf.init(keyStore, null);
      SSLContext sslContext = SSLContext.getInstance(PROTOCOL);
      sslContext.init(kmf.getKeyManagers(), null, new SecureRandom());
      return sslContext.getSocketFactory();
    }
    catch (Exception ex)
    {
      throw new RuntimeException(ex);
    }
    finally
    {
      Utils.closeSafely(inputStream);
    }
  }
}
