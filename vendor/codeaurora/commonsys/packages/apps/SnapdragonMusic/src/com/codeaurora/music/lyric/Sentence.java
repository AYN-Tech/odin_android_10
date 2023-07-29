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

public class Sentence {
    private long mFromTime;
    private long mToTime;
    private String mContent;

    public Sentence(String content, long fromTime, long toTime) {
        mContent = content;
        mFromTime = fromTime;
        mToTime = toTime;
    }

    public Sentence(String content, long fromTime) {
        this(content, fromTime, 0);
    }

    public Sentence(String content) {
        this(content, 0, 0);
    }

    public long getFromTime() {
        return mFromTime;
    }

    public void setFromTime(long fromTime) {
        mFromTime = fromTime;
    }

    public long getToTime() {
        return mToTime;
    }

    public void setToTime(long toTime) {
        mToTime = toTime;
    }

    /**
     * Analyzing the current time is being played this line lyrics.
     *
     * @param time the current time.
     * @return true if the current time is being played this line lyrics, false otherwise.
     */
    public boolean isInTime(long time) {
        return time >= mFromTime && time <= mToTime;
    }

    public String getContent() {
        return mContent;
    }

    public long getDuring() {
        return mToTime - mFromTime;
    }

    public String toString() {
        return "{" + mFromTime + "(" + mContent + ")" + mToTime + "}";
    }
}
