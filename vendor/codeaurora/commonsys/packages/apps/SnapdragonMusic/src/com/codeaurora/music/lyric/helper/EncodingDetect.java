/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.

 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.

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

package com.codeaurora.music.lyric.helper;

import java.io.BufferedInputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Array;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;

import android.util.Log;

public class EncodingDetect {
    private static final String GBK = "GBK";
    private static final String UTF_8 = "UTF-8";
    private static final String UNICODE = "UNICODE";
    private static final String UTF_16BE = "UTF-16BE";
    private static final String UTF_16LE = "UTF-16LE";
    private static final String UTF_32BE = "UTF-32BE";
    private static final String UTF_32LE = "UTF-32LE";
    private static final int READ_BYTES_LENGTH = 128;

    private ArrayList<EncodingDetectInterface> mEncodingDetectArray
            = new ArrayList<EncodingDetectInterface>();
    byte[] mByteArray = null;

    public interface EncodingDetectInterface {
        public boolean guestEncoding(byte[] bytes);

        public String getEncoding();
    }

    private EncodingDetect() {
    }

    public String getJavaEncode(File file) {
        BufferedInputStream in = null;
        try {
            FileInputStream fis = new FileInputStream(file);
            in = new BufferedInputStream(fis);
            mByteArray = new byte[READ_BYTES_LENGTH];
            in.read(mByteArray, 0, READ_BYTES_LENGTH);
            int lineSize = 0;
            for (byte tmp : mByteArray) {
                lineSize++;
                if (tmp == 13 || tmp== 10) {
                   break;
                }
            }
            byte[] line = new byte[lineSize];
            line = Arrays.copyOf(mByteArray, lineSize);
            mByteArray = line;
        } catch (IOException e) {
            e.printStackTrace();
        } finally {
            try {
                in.close();
            } catch (IOException e) {
            }
        }
        for (int i = 0; i < mEncodingDetectArray.size(); i++) {
            EncodingDetectInterface encode = mEncodingDetectArray.get(i);
            if (encode.guestEncoding(mByteArray)) {
                return encode.getEncoding();
            }
        }
        return Charset.defaultCharset().name();
    }

    private static class SingletonHolder {
        private static final EncodingDetect mEncodingDetect = new EncodingDetect();
    }

    public static final EncodingDetect getInstance() {
        return SingletonHolder.mEncodingDetect;
    }

    public void init() {
        mEncodingDetectArray.clear();
        mEncodingDetectArray.add(new UTF8EncodingDetect());
        mEncodingDetectArray.add(new UTF_16BEEncodingDetect());
        mEncodingDetectArray.add(new UTF_16LEEncodingDetect());
        mEncodingDetectArray.add(new UTF_32BEEncodingDetect());
        mEncodingDetectArray.add(new UTF_32LEEncodingDetect());
        mEncodingDetectArray.add(new UnicodeEncodingDetect());
        mEncodingDetectArray.add(new GBKEncodingDetect());
    }

    private class UTF8EncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if (bytes[0] == (byte) 0xEF && bytes[1] == (byte) 0xBB
                    && bytes[2] == (byte) 0xBF) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UTF_8;
        }
    }

    private class UnicodeEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if (bytes[0] == (byte) 0xFF && bytes[1] == (byte) 0xFE) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UNICODE;
        }
    }

    private class UTF_16BEEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if (bytes[0] == (byte) 0xFE && bytes[1] == (byte) 0xFF) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UTF_16BE;
        }
    }

    private class UTF_16LEEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if (bytes[0] == (byte) 0xFF && bytes[1] == (byte) 0xFE) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UTF_16LE;
        }
    }

    private class UTF_32BEEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if ((bytes[0] == (byte) 0x00) && (bytes[1] == (byte) 0x00)
                    && (bytes[2] == (byte) 0xFE) && (bytes[3] == (byte) 0xFF)) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UTF_32BE;
        }
    }

    private class UTF_32LEEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            if ((bytes[0] == (byte) 0xFF) && (bytes[1] == (byte) 0xFE)
                    && (bytes[2] == (byte) 0x00) && (bytes[3] == (byte) 0x00)) {
                return true;
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return UTF_32LE;
        }
    }

    private class GBKEncodingDetect implements EncodingDetectInterface {
        @Override
        public boolean guestEncoding(byte[] bytes) {
            try {
                String strGBK = new String(bytes,GBK);
                String strUTF = new String(bytes,UTF_8);
                String strGBKToUTF = new String(strGBK.getBytes(GBK),UTF_8);
                String strUTFToGBK = new String(strUTF.getBytes(UTF_8),GBK);
                if (strUTF.equals(strGBKToUTF) && !strGBK.equals(strUTFToGBK)) {
                    return true;
                }
            } catch (UnsupportedEncodingException e) {
                e.printStackTrace();
            }
            return false;
        }

        @Override
        public String getEncoding() {
            return GBK;
        }
    }

    public void release() {
        mEncodingDetectArray.clear();
    }
}

