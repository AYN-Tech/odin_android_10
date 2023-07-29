/*
Copyright (c) 2016, The Linux Foundation. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package com.android.camera.imageprocessor.filter;

import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.TotalCaptureResult;
import android.os.Handler;
import android.util.Log;

import com.android.camera.CaptureModule;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

public class ChromaflashFilter implements ImageFilter{
    public static final int NUM_REQUIRED_IMAGE = 6;
    private int mWidth;
    private int mHeight;
    private int mStrideY;
    private int mStrideVU;
    private static String TAG = "ChromaflashFilter";

    private static boolean mIsSupported = false;
    private ByteBuffer mOutBuf;
    private CaptureModule mModule;
    private int mImageNum = -1;

    private static void Log(String msg) {
        if(DEBUG) {
            Log.d(TAG, msg);
        }
    }
    public ChromaflashFilter(CaptureModule module) {
        mModule = module;
    }

    @Override
    public List<CaptureRequest> setRequiredImages(CaptureRequest.Builder builder) {
        return null;
    }

    @Override
    public String getStringName() {
        return TAG;
    }

    @Override
    public int getNumRequiredImage() {
        return NUM_REQUIRED_IMAGE;
    }

    @Override
    public void init(int width, int height, int strideY, int strideVU) {
        Log("init");
        mWidth = width/2*2;
        mHeight = height/2*2;
        mStrideY = strideY/2*2;
        mStrideVU = strideVU/2*2;
        mOutBuf = ByteBuffer.allocate(mStrideY*mHeight*3/2);
        mImageNum = -1;
        Log("width: "+mWidth+" height: "+mHeight+" strideY: "+mStrideY+" strideVU: "+mStrideVU);
        nativeInit(mWidth, mHeight, mStrideY, mStrideVU,
                0, 0, mWidth, mHeight, NUM_REQUIRED_IMAGE);
    }

    @Override
    public void deinit() {
        Log("deinit");
        mOutBuf = null;
        mImageNum = -1;
        nativeDeinit();
    }

    @Override
    public void addImage(ByteBuffer bY, ByteBuffer bVU, int imageNum, Object param) {
        Log("addImage");
        if(imageNum == 1 || imageNum == 2 || imageNum == 4) {
            mImageNum = imageNum;
            return;
        }
        int yActualSize = bY.remaining();
        int vuActualSize = bVU.remaining();
        mImageNum = imageNum;
        int status = nativeAddImage(bY, bVU, yActualSize, vuActualSize, imageNum);
        if(status != 0) {
            Log.e(TAG, "Fail to add image");
        }
    }

    @Override
    public ResultImage processImage() {
        Log("processImage ");
        int[] roi = new int[4];
        int status = nativeProcessImage(mOutBuf.array(), roi);
        Log("processImage done");
        mImageNum = -1;
        if(status < 0) { //In failure case, library will return the first image as it is.
            Log.w(TAG, "Fail to process the image.");
        }
        return new ResultImage(mOutBuf, new Rect(roi[0], roi[1], roi[0]+roi[2], roi[1] + roi[3]), mWidth, mHeight, mStrideY);
    }

    @Override
    public boolean isSupported() {
        return mIsSupported;
    }

    @Override
    public boolean isFrameListener() {
        return false;
    }

    @Override
    public boolean isManualMode() {
        return true;
    }

    @Override
    public void manualCapture(final CaptureRequest.Builder builder, final CameraCaptureSession captureSession,
                              final CameraCaptureSession.CaptureCallback callback, final Handler handler) throws CameraAccessException {
        new Thread() {
            public void run() {
                try {
                    for (int i = 0; i < NUM_REQUIRED_IMAGE; i++) {
                        if (i == 0) {
                            captureSession.capture(builder.build(), callback, handler);
                            waitForImage(i);
                        } else if (i == 1) { //To change the setting
                            builder.set(CaptureRequest.CONTROL_AE_LOCK, Boolean.FALSE);
                            builder.set(CaptureRequest.FLASH_MODE,
                                    CaptureRequest.FLASH_MODE_SINGLE);
                            captureSession.capture(builder.build(), callback, handler);
                            waitForImage(i);
                        } else if (i == 2) { //To change the setting
                            builder.set(CaptureRequest.CONTROL_AE_MODE,
                                    CaptureRequest.CONTROL_AE_MODE_ON);
                            CaptureRequest.Builder AeTunningBuilder = captureSession.getDevice().
                                    createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                            CaptureRequest request = builder.build();
                            for (CaptureRequest.Key key : request.getKeys()) {
                                AeTunningBuilder.set(key, request.get(key));
                            }
                            AeTunningBuilder.addTarget(mModule.getPreviewSurfaceForSession(
                                    mModule.getMainCameraId()));

                            waitForAeBlock(AeTunningBuilder,builder,callback,
                                    captureSession,handler,5);
                        } else if (i == 3) {
                            captureSession.capture(builder.build(), callback, handler);
                            waitForImage(i);
                        } else if (i == 4) { //To change the setting
                            builder.set(CaptureRequest.FLASH_MODE, CaptureRequest.FLASH_MODE_OFF);
                            builder.set(CaptureRequest.CONTROL_AE_LOCK, Boolean.FALSE);
                            captureSession.capture(builder.build(), callback, handler);
                            waitForImage(i);
                        } else if (i == 5) {
                            captureSession.capture(builder.build(), callback, handler);
                        }
                    }
                } catch(CameraAccessException e) {}

            }
        }.start();
    }

    private void waitForImage(int index) {
        try {
            while(mImageNum < index) {
                Thread.sleep(50);
            }
        } catch (InterruptedException e) {
        }
    }

    private void waitForAeBlock(final CaptureRequest.Builder tuningBuilder,
                                   final CaptureRequest.Builder captureBuilder,
                                   final CameraCaptureSession.CaptureCallback callback,
                                final CameraCaptureSession captureSession,
                                final Handler handler, final int AeTunningTime) {
        try{
            captureSession.capture(tuningBuilder.build(),
                    new CameraCaptureSession.CaptureCallback() {
                private boolean mAeStateConverged = false;

                @Override
                public void onCaptureCompleted(CameraCaptureSession session, CaptureRequest request,
                                               TotalCaptureResult result) {
                    Integer aeState = result.get(CaptureResult.CONTROL_AE_STATE);
                    Log.d(TAG,"AE tunning onCaptureCompleted aeState = " + aeState);
                    if (aeState != null && aeState == CaptureResult.CONTROL_AE_STATE_CONVERGED) {
                        mAeStateConverged = true;
                    }
                    Log.d(TAG,"AE tunning completed mAeStateConverged = " + mAeStateConverged);
                    if(!mAeStateConverged && AeTunningTime >= 2) {
                        int resetTime = AeTunningTime - 1;
                        waitForAeBlock(tuningBuilder,captureBuilder,callback,
                                captureSession,handler,resetTime);
                    } else {
                        try{
                            captureSession.capture(captureBuilder.build(),callback,handler);
                        } catch (CameraAccessException e){}
                    }
                }
            }, handler);
        }catch (CameraAccessException e){

        }
    }

    public static boolean isSupportedStatic() {
        return mIsSupported;
    }

    private native int nativeInit(int width, int height, int yStride, int vuStride,
                                  int roiX, int roiY, int roiW, int roiH, int numImages);
    private native int nativeDeinit();
    private native int nativeAddImage(ByteBuffer yB, ByteBuffer vuB, int ySize, int vuSize, int imageNum);
    private native int nativeProcessImage(byte[] buffer, int[] roi);

    static {
        try {
            //System.loadLibrary("jni_chromaflash");
            mIsSupported = false;
        }catch(UnsatisfiedLinkError e) {
            Log.d(TAG, e.toString());
            mIsSupported = false;
        }
    }
}
