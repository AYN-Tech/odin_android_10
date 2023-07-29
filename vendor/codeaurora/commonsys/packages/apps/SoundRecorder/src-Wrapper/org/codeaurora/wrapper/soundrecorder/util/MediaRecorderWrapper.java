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

package org.codeaurora.wrapper.soundrecorder.util;

import android.media.MediaRecorder;


public class MediaRecorderWrapper {

    public static final int MEDIA_RECORDER_INFO_MAX_DURATION_REACHED = MediaRecorder.MEDIA_RECORDER_INFO_MAX_DURATION_REACHED;


    public final class AudioSource {

        private AudioSource() {}

        public final static int AUDIO_SOURCE_INVALID = MediaRecorder.AudioSource.AUDIO_SOURCE_INVALID;

        /** Default audio source **/
        public static final int DEFAULT = MediaRecorder.AudioSource.DEFAULT;

        /** Microphone audio source */
        public static final int MIC = MediaRecorder.AudioSource.MIC;

        /** Voice call uplink (Tx) audio source */
        public static final int VOICE_UPLINK = MediaRecorder.AudioSource.VOICE_UPLINK;

        /** Voice call downlink (Rx) audio source */
        public static final int VOICE_DOWNLINK = MediaRecorder.AudioSource.VOICE_DOWNLINK;

        /** Voice call uplink + downlink audio source */
        public static final int VOICE_CALL = MediaRecorder.AudioSource.VOICE_CALL;

        /** Microphone audio source with same orientation as camera if available, the main
         *  device microphone otherwise */
        public static final int CAMCORDER = MediaRecorder.AudioSource.CAMCORDER;

        public static final int VOICE_RECOGNITION = MediaRecorder.AudioSource.VOICE_RECOGNITION;

        public static final int VOICE_COMMUNICATION = MediaRecorder.AudioSource.VOICE_COMMUNICATION;

        public static final int REMOTE_SUBMIX = MediaRecorder.AudioSource.REMOTE_SUBMIX;

        public static final int UNPROCESSED = MediaRecorder.AudioSource.UNPROCESSED;

        /**
         * Audio source for capturing broadcast radio tuner output.
         */
        public static final int RADIO_TUNER = MediaRecorder.AudioSource.RADIO_TUNER;

        public static final int HOTWORD = MediaRecorder.AudioSource.HOTWORD;
    }

    public final class AudioEncoder {
      /* Do not change these values without updating their counterparts
       * in include/media/mediarecorder.h!
       */
        private AudioEncoder() {}
        public static final int DEFAULT = MediaRecorder.AudioEncoder.DEFAULT;
        /** AMR (Narrowband) audio codec */
        public static final int AMR_NB = MediaRecorder.AudioEncoder.AMR_NB;
        /** AMR (Wideband) audio codec */
        public static final int AMR_WB = MediaRecorder.AudioEncoder.AMR_WB;
        /** AAC Low Complexity (AAC-LC) audio codec */
        public static final int AAC = MediaRecorder.AudioEncoder.AAC;
        /** High Efficiency AAC (HE-AAC) audio codec */
        public static final int HE_AAC = MediaRecorder.AudioEncoder.HE_AAC;
        /** Enhanced Low Delay AAC (AAC-ELD) audio codec */
        public static final int AAC_ELD = MediaRecorder.AudioEncoder.AAC_ELD;
        /** Ogg Vorbis audio codec */
        public static final int VORBIS = MediaRecorder.AudioEncoder.VORBIS;
        /**  EVRC audio codec */
        public static final int EVRC = 10;//MediaRecorder.AudioEncoder.EVRC;
        /**  QCELP audio codec */
        public static final int QCELP = 11;//MediaRecorder.AudioEncoder.QCELP;
        /**  Linear PCM audio codec */
        public static final int LPCM = 12;//MediaRecorder.AudioEncoder.LPCM;
        /**  MPEGH audio codec */
        public static final int MPEGH = 13;//MediaRecorder.AudioEncoder.MPEGH;
    }

    public final class OutputFormat {
      /* Do not change these values without updating their counterparts
       * in include/media/mediarecorder.h!
       */
        private OutputFormat() {}

        public static final int DEFAULT = MediaRecorder.OutputFormat.DEFAULT;
        /** 3GPP media file format*/
        public static final int THREE_GPP = MediaRecorder.OutputFormat.THREE_GPP;
        /** MPEG4 media file format*/
        public static final int MPEG_4 = MediaRecorder.OutputFormat.MPEG_4;

        /** The following formats are audio only .aac or .amr formats */

        /**
         * AMR NB file format
         *  Deprecated in favor of MediaRecorder.OutputFormat.AMR_NB
         */
        public static final int RAW_AMR = MediaRecorder.OutputFormat.RAW_AMR;

        /** AMR NB file format */
        public static final int AMR_NB = MediaRecorder.OutputFormat.AMR_NB;

        /** AMR WB file format */
        public static final int AMR_WB = MediaRecorder.OutputFormat.AMR_WB;

        /** AAC ADIF file format */
        public static final int AAC_ADIF = MediaRecorder.OutputFormat.AAC_ADIF;

        /** AAC ADTS file format */
        public static final int AAC_ADTS = MediaRecorder.OutputFormat.AAC_ADTS;

        /**  Stream over a socket, limited to a single stream */
        public static final int OUTPUT_FORMAT_RTP_AVP = MediaRecorder.OutputFormat.OUTPUT_FORMAT_RTP_AVP;

        /** H.264/AAC data encapsulated in MPEG2/TS */
        public static final int OUTPUT_FORMAT_MPEG2TS = 8;//MediaRecorder.OutputFormat.OUTPUT_FORMAT_MPEG2TS;

        /** VP8/VORBIS data in a WEBM container */
        public static final int WEBM = MediaRecorder.OutputFormat.WEBM;

        /** QCP file format */
        public static final int QCP = 20;//MediaRecorder.OutputFormat.QCP;

        /** WAVE media file format*/
        public static final int WAVE = 21;//MediaRecorder.OutputFormat.WAVE;

    };

}
