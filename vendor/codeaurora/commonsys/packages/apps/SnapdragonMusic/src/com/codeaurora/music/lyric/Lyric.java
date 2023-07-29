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

package com.codeaurora.music.lyric;

import com.codeaurora.music.lyric.helper.EncodingDetect;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class Lyric {
    public static final String TAG = "Lyric";
    private int mOffset;
    private long mTotalTime;
    private transient PlayListItem mItemInfo;
    public List<Sentence> mSentenceList = new ArrayList<Sentence>();
    private static final Pattern mPattern = Pattern.compile("(?<=\\[).*?(?=\\])");
    private static final String OFFSET = "offset";
    private static EncodingDetect mEncodingDetect;

    public Lyric(File lyricFile, PlayListItem itemInfo, long totalTime) {
        mOffset = itemInfo.getOffset();
        mItemInfo = itemInfo;
        mTotalTime = totalTime;
        initLyric(lyricFile);
    }

    public Lyric(String lyricFile, PlayListItem itemInfo) {
        mOffset = itemInfo.getOffset();
        mItemInfo = itemInfo;
        initLyric(lyricFile);
    }

    private void initLyric(File lyricFile) {
        BufferedReader buf = null;
        try {
            if (mEncodingDetect == null) {
                mEncodingDetect = EncodingDetect.getInstance();
                mEncodingDetect.init();
            }
            String encodeType = mEncodingDetect.getJavaEncode(lyricFile);
            buf = new BufferedReader(new InputStreamReader(
                    new FileInputStream(lyricFile), encodeType));
            StringBuilder strBuilder = new StringBuilder();
            String tmp = null;
            while ((tmp = buf.readLine()) != null) {
                strBuilder.append(tmp).append("\n");
            }
            initLyric(strBuilder.toString());
        } catch (Exception ex) {
        } finally {
            try {
                buf.close();
            } catch (Exception ex) {
            }
        }
    }

    private void initLyric(String LyricContent) {
        if (LyricContent == null || LyricContent.trim().equals("")) {
            mSentenceList.add(new Sentence(mItemInfo.getFormattedName(), Integer.MIN_VALUE,
                    Integer.MAX_VALUE));
            return;
        }
        try {
            BufferedReader buf = new BufferedReader(new StringReader(LyricContent));
            String tmp = null;
            while ((tmp = buf.readLine()) != null) {
                parseLyricLine(tmp.trim());
            }
            buf.close();
            Collections.sort(mSentenceList, new Comparator<Sentence>() {

                public int compare(Sentence sentence1, Sentence sentence2) {
                    return (int) (sentence1.getFromTime() - sentence2.getFromTime());
                }
            });
            if (mSentenceList.size() == 0) {
                mSentenceList.add(new Sentence(mItemInfo.getFormattedName(), 0, Integer.MAX_VALUE));
                return;
            }

            int size = mSentenceList.size();
            for (int i = 0; i < size; i++) {
                Sentence nextSentence = null;
                if (i + 1 < size) {
                    nextSentence = mSentenceList.get(i + 1);
                }
                Sentence tmpSentence = mSentenceList.get(i);
                if (nextSentence != null) {
                    tmpSentence.setToTime(nextSentence.getFromTime() - 1);
                }
            }
            if (mSentenceList.size() == 1) {
                mSentenceList.get(0).setToTime(Integer.MAX_VALUE);
            } else {
                Sentence lastSentence = mSentenceList.get(mSentenceList.size() - 1);
                lastSentence.setToTime(mTotalTime);
            }
        } catch (Exception ex) {
        }
    }

    private void parseLyricLine(String lineLyric) {
        if (lineLyric.equals("")) {
            return;
        }
        int lastIndex = -1;
        int lastLength = -1;
        Matcher matcher = mPattern.matcher(lineLyric);
        List<String> tmpList = new ArrayList<String>();
        while (matcher.find()) {
            String strTmp = matcher.group();
            int index = lineLyric.indexOf("[" + strTmp + "]");
            if (lastIndex != -1 && index - lastIndex > lastLength + 2) {
                String content = lineLyric.substring(lastIndex + lastLength + 2, index);
                for (String str : tmpList) {
                    long time = parseTime(str);
                    if (time != -1) {
                        mSentenceList.add(new Sentence(content, time));
                    }
                }
                tmpList.clear();
            }
            tmpList.add(strTmp);
            lastIndex = index;
            lastLength = strTmp.length();
        }
        if (tmpList.isEmpty()) {
            return;
        }
        try {
            int length = lastLength + 2 + lastIndex;
            String content = lineLyric.substring(length > lineLyric.length()
                    ? lineLyric.length() : length);
            if (content.equals("") && mOffset == 0) {
                for (String strTmp : tmpList) {
                    int of = parseOffset(strTmp);
                    if (of != Integer.MAX_VALUE) {
                        mOffset = of;
                        mItemInfo.setOffset(mOffset);
                        break;
                    }
                }
                return;
            }
            for (String strTmp : tmpList) {
                long t = parseTime(strTmp);
                if (t != -1) {
                    mSentenceList.add(new Sentence(content, t));
                }
            }
        } catch (Exception exe) {
        }
    }

    private int parseOffset(String str) {
        String[] strArray = str.split("\\:");
        if (strArray.length == 2) {
            if (strArray[0].equalsIgnoreCase(OFFSET)) {
                int val = Integer.parseInt(strArray[1]);
                return val;
            } else {
                return Integer.MAX_VALUE;
            }
        } else {
            return Integer.MAX_VALUE;
        }
    }

    private long parseTime(String time) {
        String[] strArray = time.split("\\:|\\.");
        if (strArray.length < 2) {
            return -1;
        } else if (strArray.length == 2) {
            try {
                if (mOffset == 0 && strArray[0].equalsIgnoreCase("offset")) {
                    mOffset = Integer.parseInt(strArray[1]);
                    mItemInfo.setOffset(mOffset);
                    return -1;
                }
                int min = Integer.parseInt(strArray[0]);
                int sec = Integer.parseInt(strArray[1]);
                if (min < 0 || sec < 0 || sec >= 60) {
                    throw new RuntimeException("Parse time error");
                }
                return (min * 60 + sec) * 1000L;
            } catch (Exception ex) {
                return -1;
            }
        } else if (strArray.length == 3) {
            try {
                int min = Integer.parseInt(strArray[0]);
                int sec = Integer.parseInt(strArray[1]);
                int mm = Integer.parseInt(strArray[2]);
                if (min < 0 || sec < 0 || sec >= 60 || mm < 0 || mm > 99) {
                    throw new RuntimeException("Parse time error");
                }
                return (min * 60 + sec) * 1000L + mm * 10;
            } catch (Exception exe) {
                return -1;
            }
        } else {
            return -1;
        }
    }

    public void release() {
        if (mEncodingDetect != null) {
            mEncodingDetect.release();
            mEncodingDetect = null;
        }
    }
}
