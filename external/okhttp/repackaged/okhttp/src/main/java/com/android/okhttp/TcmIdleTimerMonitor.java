/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package com.android.okhttp;

import com.android.okhttp.internal.Platform;
import dalvik.system.PathClassLoader;
import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.lang.reflect.InvocationHandler;

/**
 * @hide
 */


public class TcmIdleTimerMonitor implements InvocationHandler {
  private ConnectionPool connectionPool;
  private static Object tcmClient = null;
  private static Method mTcmRegisterMethod = null;
  private static Object lockObj = new Object();
  private static Object dpmTcmIface;
  Object result = null;

  /** @hide */
  public TcmIdleTimerMonitor(ConnectionPool connPool) {
    synchronized(lockObj) {
      this.connectionPool = connPool;
      try {
        if (mTcmRegisterMethod == null || tcmClient == null) {
          //load tcm if not already loaded
          PathClassLoader tcmClassLoader =
            new PathClassLoader("/system/framework/tcmclient.jar",
                ClassLoader.getSystemClassLoader());
          //load tcmiface if not already loader
          PathClassLoader tcmIfaceClassLoader =
            new PathClassLoader("/system/framework/tcmiface.jar",
                ClassLoader.getSystemClassLoader());

          Class tcmClass = tcmClassLoader.loadClass("com.qti.tcmclient.DpmTcmClient");

          Class tcmIfaceClass = tcmIfaceClassLoader.loadClass("com.quicinc.tcmiface.DpmTcmIface");
          //Platform.get().logW("print class name: " + tcmIfaceClass.getName());

          //get DpmTcmIface object via ProxyInstance
          dpmTcmIface =  Proxy.newProxyInstance(tcmIfaceClassLoader,
                      new Class[]{tcmIfaceClass}, this);

          Method mGetTcmMethod = tcmClass.getDeclaredMethod("getInstance");
          tcmClient = mGetTcmMethod.invoke(null);

          mTcmRegisterMethod = tcmClass.getDeclaredMethod("registerTcmMonitor",Object.class);

          //Platform.get().logW("tcmiface methoid  value: " + mTcmRegisterMethod );
        }
        if (mTcmRegisterMethod != null && tcmClient != null) {
          result = mTcmRegisterMethod.invoke(tcmClient, dpmTcmIface);
        }
      } catch (ClassNotFoundException e) {
        //Ignore ClassNotFound Exception
      } catch (Exception e) {
         Platform.get().logW("tcmclient load failed: " + e);
      }
    }
  }


  /** @hide */
  public Object invoke(Object proxy, Method method, Object[] args)
          throws Throwable {

        //Platform.get().logW("tcmclient invoke called : onCloseIdleConnection");
          this.OnCloseIdleConn();
          return null;
   }

  /** @hide */
  public void OnCloseIdleConn()
  {
     if (connectionPool != null) {
       //Platform.get().logW("tcmclient invoke called : onCloseIdleConnection calling next level");
       connectionPool.closeIdleConnections();
     }
  }
} // class TcmIdleTimerMonitor
