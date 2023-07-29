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

public class PlayListItem {
    protected String mName = "";
    protected boolean mIsFile = true;
    protected long mSeconds = -1;
    private String mTitle;
    private int mOffset;

    /**
     *
     * @param name     Song name to be displayed
     * @param seconds  Time length
     * @param isFile   true for File instance
     */
    public PlayListItem(String name, long seconds, boolean isFile) {
        mName = name;
        mSeconds = seconds;
        mIsFile = isFile;
    }

    public int getOffset() {
        return mOffset;
    }

    public void setOffset(int offset) {
        mOffset = offset;
    }

    public void setDuration(long sec) {
        mSeconds = sec;
    }

    public String getFormattedName() {
        return mName;
    }

    public String getName() {
        return mName;
    }

    public boolean isFile() {
        return mIsFile;
    }

    public void setFile(boolean b) {
        mIsFile = b;
    }

    public long getLength() {
        return mSeconds;
    }

    public String getTitle() {
        if (mTitle != null) {
            return mTitle;
        } else {
            mTitle = mName;
        }
        return mTitle;
    }

    public void setTitle(String title) {
        mTitle = title;
    }
}
