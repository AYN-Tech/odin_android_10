/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.mms.util;

import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.ui.LetterTileDrawable;

import android.graphics.PorterDuffXfermode;
import android.graphics.PorterDuff;
import android.graphics.Paint;
import android.graphics.Rect;
import android.graphics.RectF;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.os.Parcel;
import android.os.Parcelable;
import android.os.Trace;
import android.text.TextUtils;

public class MaterialColorMapUtils {
    private final TypedArray sPrimaryColors;
    private final TypedArray sSecondaryColors;

    public MaterialColorMapUtils(Resources resources) {
        sPrimaryColors = resources.obtainTypedArray(R.array.letter_tile_colors);
        sSecondaryColors = resources.obtainTypedArray(R.array.letter_tile_colors_dark);
    }

    public static class MaterialPalette implements Parcelable {
        public MaterialPalette(int primaryColor, int secondaryColor) {
            mPrimaryColor = primaryColor;
            mSecondaryColor = secondaryColor;
        }
        public final int mPrimaryColor;
        public final int mSecondaryColor;

        @Override
        public boolean equals(Object obj) {
            if (this == obj) {
                return true;
            }
            if (obj == null) {
                return false;
            }
            if (getClass() != obj.getClass()) {
                return false;
            }
            MaterialPalette other = (MaterialPalette) obj;
            if (mPrimaryColor != other.mPrimaryColor) {
                return false;
            }
            if (mSecondaryColor != other.mSecondaryColor) {
                return false;
            }
            return true;
        }

        @Override
        public int hashCode() {
            final int prime = 31;
            int result = 1;
            result = prime * result + mPrimaryColor;
            result = prime * result + mSecondaryColor;
            return result;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(mPrimaryColor);
            dest.writeInt(mSecondaryColor);
        }

        private MaterialPalette(Parcel in) {
            mPrimaryColor = in.readInt();
            mSecondaryColor = in.readInt();
        }

        public static final Creator<MaterialPalette> CREATOR = new Creator<MaterialPalette>() {
                @Override
                public MaterialPalette createFromParcel(Parcel in) {
                    return new MaterialPalette(in);
                }

                @Override
                public MaterialPalette[] newArray(int size) {
                    return new MaterialPalette[size];
                }
        };
    }

    /**
     * Return primary and secondary colors from the Material color palette that are similar to
     * {@param color}.
     */
    public MaterialPalette calculatePrimaryAndSecondaryColor(int color) {
        Trace.beginSection("calculatePrimaryAndSecondaryColor");

        final float colorHue = hue(color);
        float minimumDistance = Float.MAX_VALUE;
        int indexBestMatch = 0;
        for (int i = 0; i < sPrimaryColors.length(); i++) {
            final int primaryColor = sPrimaryColors.getColor(i, 0);
            final float comparedHue = hue(primaryColor);
            // No need to be perceptually accurate when calculating color distances since
            // we are only mapping to 15 colors. Being slightly inaccurate isn't going to change
            // the mapping very often.
            final float distance = Math.abs(comparedHue - colorHue);
            if (distance < minimumDistance) {
                minimumDistance = distance;
                indexBestMatch = i;
            }
        }

        Trace.endSection();
        return new MaterialPalette(sPrimaryColors.getColor(indexBestMatch, 0),
                sSecondaryColors.getColor(indexBestMatch, 0));
    }

    public static MaterialPalette getDefaultPrimaryAndSecondaryColors(Resources resources) {
        final int primaryColor = resources.getColor(
                R.color.quickcontact_default_photo_tint_color);
        final int secondaryColor = resources.getColor(
                R.color.quickcontact_default_photo_tint_color_dark);
        return new MaterialPalette(primaryColor, secondaryColor);
    }

    /**
     * Returns the hue component of a color int.
     *
     * @return A value between 0.0f and 1.0f
     */
    public static float hue(int color) {
        int r = (color >> 16) & 0xFF;
        int g = (color >> 8) & 0xFF;
        int b = color & 0xFF;

        int V = Math.max(b, Math.max(r, g));
        int temp = Math.min(b, Math.min(r, g));

        float H;

        if (V == temp) {
            H = 0;
        } else {
            final float vtemp = V - temp;
            final float cr = (V - r) / vtemp;
            final float cg = (V - g) / vtemp;
            final float cb = (V - b) / vtemp;

            if (r == V) {
                H = cb - cg;
            } else if (g == V) {
                H = 2 + cr - cb;
            } else {
                H = 4 + cg - cr;
            }

            H /= 6.f;
            if (H < 0) {
                H++;
            }
        }

        return H;
    }

    public static Drawable getLetterTitleDraw(Context mContext, Contact contact) {
        final Resources res = mContext.getResources();
        final LetterTileDrawable drawable = new LetterTileDrawable(mContext, res);

        String mAvatarName = null;
        String lookup = null;
        if (contact != null) {
            mAvatarName = contact.getNameForAvatar();
            lookup = contact.getLookup();
            if (TextUtils.isEmpty(lookup)) {
                lookup = contact.getNumber();
            }
        }
        drawable.setContactDetails(mAvatarName, lookup);
        int mColor = drawable.getColor();

        if (contact != null) {
            contact.setContactColor(mColor);
        }
        drawable.setContactType(LetterTileDrawable.TYPE_DEFAULT);
        drawable.setScale(1.0f);
        drawable.setIsCircular(true);
        return drawable;
    }

    public static Bitmap toConformBitmap(Bitmap background, Bitmap foreground) {
        if( background == null ) {
            return null;
        }

        int bgWidth = background.getWidth();
        int bgHeight = background.getHeight();
        Bitmap newbmp = Bitmap.createBitmap(bgWidth, bgHeight, Bitmap.Config.ARGB_8888);
        Canvas cv = new Canvas(newbmp);
        cv.drawBitmap(background, 0, 0, null);
        cv.drawBitmap(foreground, bgWidth / 4, bgHeight / 4, null);
        return newbmp;
    }

    public static Bitmap getLetterTitleDraw(Context mContext, Contact contact, int size) {
        if (size <= 0) return null;

        Drawable drawable = getLetterTitleDraw(mContext, contact);

        int w = size;
        int h = size;
        Bitmap.Config config = drawable.getOpacity() != android.graphics.PixelFormat.OPAQUE
                ? Bitmap.Config.ARGB_8888 : Bitmap.Config.RGB_565;
        Bitmap bitmap = Bitmap.createBitmap(w, h, config);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, w, h);
        drawable.draw(canvas);
        return bitmap;
    }

    public static Bitmap vectobitmap (Drawable drawable, int size) {
        if (size <= 0) return null;

        int w = size;
        int h = size;
        Bitmap.Config config = drawable.getOpacity() != android.graphics.PixelFormat.OPAQUE
                ? Bitmap.Config.ARGB_8888 : Bitmap.Config.RGB_565;
        Bitmap bitmap = Bitmap.createBitmap(w, h, config);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, w, h);
        drawable.draw(canvas);
        return bitmap;
    }

    public static Bitmap getCircularBitmap(Context context, Bitmap bitmap) {
        final int outpustSize = context.getResources().getDimensionPixelSize(
                android.R.dimen.notification_large_icon_height);
        final Bitmap output = Bitmap
                .createBitmap(outpustSize, outpustSize, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(output);
        final Rect dest = new Rect(0, 0, outpustSize, outpustSize);
        final RectF oval = new RectF(dest);
        final Paint paint = new Paint();
        paint.setAntiAlias(true);
        canvas.drawOval(oval, paint);
        paint.setXfermode(new PorterDuffXfermode(PorterDuff.Mode.SRC_IN));
        canvas.drawBitmap(bitmap, null, dest, paint);
        return output;
    }
}
