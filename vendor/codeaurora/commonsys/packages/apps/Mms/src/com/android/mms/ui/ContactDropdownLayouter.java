/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
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
 *
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

package com.android.mms.ui;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.StateListDrawable;
import android.provider.Telephony;
import android.support.v4.text.BidiFormatter;
import android.support.v4.text.TextDirectionHeuristicsCompat;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import com.android.ex.chips.DropdownChipLayouter;
import com.android.ex.chips.RecipientEntry;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.util.ContactRecipientEntryUtils;
import com.android.mms.util.MaterialColorMapUtils;

/**
 * An implementation for {@link DropdownChipLayouter}.
 */
public class ContactDropdownLayouter extends DropdownChipLayouter {
    private Context mContext;
    private RecipientEntry mEntry;
    private Drawable mDefaultContactImage;
    private Drawable mDefaultGroupContactImage;

    public ContactDropdownLayouter(final LayoutInflater inflater, final Context context) {
        super(inflater, context);
        mContext = context;
        mDefaultContactImage = mContext.getResources().getDrawable(R.drawable.myavatar);
        mDefaultGroupContactImage = context.getResources().getDrawable(R.drawable.group_avatar);
    }

    /**
     * Bind a drop down view to a RecipientEntry. We'd like regular dropdown items (BASE_RECIPIENT)
     * to behave the same as regular ContactListItemViews, while using the chips library's item
     * styling for alternates dropdown items (happens when you click on a chip).
     */
    @Override
    public View bindView(final View convertView, final ViewGroup parent,
            final RecipientEntry entry,
            final int position, AdapterType type, final String substring,
            StateListDrawable deleteDrawable) {
        mEntry = entry;
        deleteDrawable = (StateListDrawable) mContext.getResources().getDrawable(
                R.drawable.delete_chip);
        return super.bindView(convertView, parent, entry, position, type, substring,
                deleteDrawable);
    }

    @Override
    protected void bindTextToView(CharSequence text, TextView view) {
        if (view == null) {
            return;
        }
        if (mEntry.getContactId()
                == ContactRecipientEntryUtils.CONTACT_ID_SENDTO_DESTINATION) {
            if (view.getId() == android.R.id.title) {
                final BidiFormatter bidiFormatter = BidiFormatter.getInstance();
                final String displayName = bidiFormatter.unicodeWrap(
                        ContactRecipientEntryUtils.getDisplayNameForContactList(mContext, mEntry),
                        TextDirectionHeuristicsCompat.LTR);
                super.bindTextToView(displayName, view);
            } else {
                view.setVisibility(View.GONE);
            }
        } else if (mEntry.getContactId()
                == ContactRecipientEntryUtils.CONTACT_ID_GROUP_DESTINATION) {
            if (view.getId() == android.R.id.text2) {
                view.setVisibility(View.GONE);
            } else {
                super.bindTextToView(text, view);
            }
        } else {
            if (view.getId() == android.R.id.text2) {
                if (text != null) {
                    char c = text.charAt(0);
                    if (Character.isLetter(c)) {
                        text = text.subSequence(0,1)+text.subSequence(1,text.length())
                                .toString().toLowerCase();
                    }
                }
            }
            super.bindTextToView(text, view);
        }
    }

    @Override
    protected void bindIconToView(boolean showImage, RecipientEntry entry, ImageView view,
            AdapterType type) {
        if (view == null) {
            return;
        }
        if (showImage && view.getId() == android.R.id.icon) {
            Drawable avatarDrawable;
            Drawable backgroundDrawable;
            if (mEntry.getContactId()
                    == ContactRecipientEntryUtils.CONTACT_ID_GROUP_DESTINATION) {
                String groupName = mEntry.getDisplayName();
                if (LetterTileDrawable.isEnglishLetterString(groupName)) {
                    backgroundDrawable = MaterialColorMapUtils.getLetterTitleDraw(mContext, null);
                    view.setBackgroundDrawable(backgroundDrawable);
                }
                view.setScaleType(ImageButton.ScaleType.CENTER);
                view.setImageDrawable(mDefaultGroupContactImage);
            } else {
                String destination = entry.getDestination();
                Contact contact = Contact.get(destination, false);
                avatarDrawable = contact.getAvatar(mContext, mDefaultContactImage);
                if (contact.existsInDatabase()) {
                    view.setScaleType(ImageButton.ScaleType.FIT_CENTER);
                    if (avatarDrawable.equals(mDefaultContactImage)) {
                        if (LetterTileDrawable.isEnglishLetterString(contact.getNameForAvatar())) {
                            avatarDrawable = MaterialColorMapUtils
                                    .getLetterTitleDraw(mContext, contact);
                        } else {
                            backgroundDrawable = MaterialColorMapUtils.getLetterTitleDraw(mContext,
                                    contact);
                            view.setBackgroundDrawable(backgroundDrawable);
                            view.setScaleType(ImageButton.ScaleType.CENTER);
                        }
                    } else {
                        view.setScaleType(ImageView.ScaleType.CENTER_CROP);
                        view.setBackgroundDrawable(null);
                    }

                    view.setImageDrawable(avatarDrawable);
                } else {
                    // identify it is phone number or email address,handle it respectively
                    view.setScaleType(ImageButton.ScaleType.CENTER);
                    view.setBackgroundResource(R.drawable.send_to_avatar_background);
                }
                view.setImageDrawable(avatarDrawable);
            }
            view.setVisibility(View.VISIBLE);
        } else {
            super.bindIconToView(showImage, entry, view, type);
            view.setVisibility(View.INVISIBLE);
        }
    }

    @Override
    protected int getItemLayoutResId(AdapterType type) {
        switch (type) {
            case BASE_RECIPIENT:
                return R.layout.mms_chips_recipient_dropdown_item;
            case RECIPIENT_ALTERNATES:
                return R.layout.chips_recipient_alternates_item;
            default:
                return R.layout.chips_recipient_alternates_item;
        }
    }

    @Override
    protected int getAlternateItemLayoutResId(AdapterType type) {
        return getItemLayoutResId(type);
    }
}
