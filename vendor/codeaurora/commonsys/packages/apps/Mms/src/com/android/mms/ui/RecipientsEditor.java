/*
 * Copyright (C) 2008 Esmertec AG.
 * Copyright (C) 2008 The Android Open Source Project
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

package com.android.mms.ui;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import android.content.Context;
import android.telephony.PhoneNumberUtils;
import android.text.Annotation;
import android.text.Editable;
import android.text.Layout;
import android.text.Spannable;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.text.util.Rfc822Token;
import android.text.util.Rfc822Tokenizer;
import android.util.AttributeSet;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.AdapterView;
import android.widget.MultiAutoCompleteTextView;

import com.android.ex.chips.DropdownChipLayouter;
import com.android.ex.chips.RecipientEditTextView;
import com.android.ex.chips.RecipientEntry;
import com.android.mms.MmsConfig;
import com.android.mms.R;
import com.android.mms.data.Contact;
import com.android.mms.data.ContactList;
import com.android.mms.util.ContactRecipientEntryUtils;

/**
 * Provide UI for editing the recipients of multi-media messages.
 */
public class RecipientsEditor extends RecipientEditTextView {
    private static final char SBC_CHAR_START = 65281;
    private static final char SBC_CHAR_END = 65373;
    private static final int DROP_DOWN_WIDTH = 1080;
    private static final int DROP_DOWN_VERTICAL_OFFSET = 40;

    private int mLongPressedPosition = -1;
    private final RecipientsEditorTokenizer mTokenizer;
    private char mLastSeparator = ',';
    private Runnable mOnSelectChipRunnable;
    private final AddressValidator mInternalValidator;
    private Context mContext;
    private int mRecipientsCount = -1;

    /** A noop validator that does not munge invalid texts and claims any address is valid */
    private class AddressValidator implements Validator {
        public CharSequence fixText(CharSequence invalidText) {
            return invalidText;
        }

        public boolean isValid(CharSequence text) {
            return true;
        }
    }

    public RecipientsEditor(Context context, AttributeSet attrs) {
        super(context, attrs);

        mContext = context;
        mTokenizer = new RecipientsEditorTokenizer();
        setTokenizer(mTokenizer);

        mInternalValidator = new AddressValidator();
        super.setValidator(mInternalValidator);

        // For the focus to move to the message body when soft Next is pressed
        setImeOptions(EditorInfo.IME_ACTION_NEXT);

        setThreshold(1);    // pop-up the list after a single char is typed

        /*
         * The point of this TextWatcher is that when the user chooses
         * an address completion from the AutoCompleteTextView menu, it
         * is marked up with Annotation objects to tie it back to the
         * address book entry that it came from.  If the user then goes
         * back and edits that part of the text, it no longer corresponds
         * to that address book entry and needs to have the Annotations
         * claiming that it does removed.
         */
        addTextChangedListener(new TextWatcher() {
            private Annotation[] mAffected;

            @Override
            public void beforeTextChanged(CharSequence s, int start,
                    int count, int after) {
                mAffected = ((Spanned) s).getSpans(start, start + count,
                        Annotation.class);
            }

            @Override
            public void onTextChanged(CharSequence s, int start,
                    int before, int after) {
                // inserting a character
                if (before == 0 && after == 1 &&
                        start >= 0 && start < s.length()) {
                    char c = s.charAt(start);
                    if (c == ',' || c == ';') {
                        // Remember the delimiter the user typed to end this recipient. We'll
                        // need it shortly in terminateToken().
                        mLastSeparator = c;
                    }
                }
            }

            @Override
            public void afterTextChanged(Editable s) {
                if (mAffected != null) {
                    for (Annotation a : mAffected) {
                        s.removeSpan(a);
                    }
                }
                mAffected = null;
            }
        });

        setDropdownChipLayouter(new ContactDropdownLayouter(
                LayoutInflater.from(context), context));
        setDropDownWidth(DROP_DOWN_WIDTH);
        setDropDownVerticalOffset(DROP_DOWN_VERTICAL_OFFSET);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        if (position < 0) {
            return;
        }
        RecipientEntry item = getAdapter().getItem(position);
        if (item == null) {
            return;
        }
        if (item.getEntryType() == RecipientEntry.ENTRY_TYPE_PERSON) {
            super.onItemClick(parent, view, position, id);
        }

        if (mOnSelectChipRunnable != null) {
            mOnSelectChipRunnable.run();
        }
    }

    public void setOnSelectChipRunnable(Runnable onSelectChipRunnable) {
        mOnSelectChipRunnable = onSelectChipRunnable;
    }

    @Override
    public boolean enoughToFilter() {
        if (!super.enoughToFilter()) {
            return false;
        }
        // If the user is in the middle of editing an existing recipient, don't offer the
        // auto-complete menu. Without this, when the user selects an auto-complete menu item,
        // it will get added to the list of recipients so we end up with the old before-editing
        // recipient and the new post-editing recipient. As a precedent, gmail does not show
        // the auto-complete menu when editing an existing recipient.
        int end = getSelectionEnd();
        int len = getText().length();

        return end == len;

    }

    public int getRecipientCount() {
        if (mRecipientsCount == -1) {
            return mTokenizer.getNumbers().size();
        } else {
            return mRecipientsCount;
        }
    }

    public List<String> getNumbers() {
        return mTokenizer.getNumbers();
    }

    public String getExsitNumbers(){
        return mTokenizer.getNumbersString();
    }

    public ContactList constructContactsFromInput(boolean blocking) {
        List<String> numbers = mTokenizer.getNumbers();
        ContactList list = new ContactList();
        for (String number : numbers) {
            Contact contact = Contact.get(number, blocking);
            contact.setNumber(number);
            list.add(contact);
        }
        return list;
    }

    private boolean isValidAddress(String number, boolean isMms) {
        if (isMms) {
            return MessageUtils.isValidMmsAddress(number);
        } else {
            if (hasInvalidCharacter(number)) {
                return false;
            }

            // TODO: PhoneNumberUtils.isWellFormedSmsAddress() only check if the number is a valid
            // GSM SMS address. If the address contains a dialable char, it considers it a well
            // formed SMS addr. CDMA doesn't work that way and has a different parser for SMS
            // address (see CdmaSmsAddress.parse(String address)). We should definitely fix this!!!
            return PhoneNumberUtils.isWellFormedSmsAddress(number)
                    || ContactRecipientEntryUtils.isEmailAddress(number);
        }
    }

    /**
     * Return true if the number contains invalid character.
     */
    private boolean hasInvalidCharacter(String number) {
        char[] charNumber = number.trim().toCharArray();
        int count = charNumber.length;
        if (mContext.getResources().getBoolean(R.bool.config_filter_char_address)) {
            for (int i = 0; i < count; i++) {
                // Allow first character is + character
                if (i == 0 && charNumber[i] == '+') {
                    continue;
                }
                if (!isValidCharacter(charNumber[i])) {
                    return true;
                }
            }
        } else {
            for (int i = 0; i < count; i++) {
                if (isSBCCharacter(charNumber, i)) {
                    return true;
                }
            }
        }
        return false;
    }

    /**
    * Return true if the charNumber belongs full-width characters
    */
    private boolean isSBCCharacter(char[] charNumber, int i) {
        return charNumber[i] >= SBC_CHAR_START && charNumber[i] <= SBC_CHAR_END;
    }

    private boolean isValidCharacter(char c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '(' || c == ')' || c == ' ';
    }

    public int getValidRecipientsCount(boolean isMms) {
        int validNum = 0;
        int invalidNum = 0;
        for (String number : mTokenizer.getNumbers()) {
            if (isValidAddress(number, isMms)) {
                validNum++;
            } else {
                invalidNum++;
            }
        }
        int count = mTokenizer.getNumbers().size();
        if (validNum == count) {
            return MessageUtils.ALL_RECIPIENTS_VALID;
        } else if (invalidNum == count) {
            return MessageUtils.ALL_RECIPIENTS_INVALID;
        }
        return invalidNum;

    }

    public boolean hasInvalidRecipient(boolean isMms) {
        for (String number : mTokenizer.getNumbers()) {
            if (!isValidAddress(number, isMms)) {
                if (MmsConfig.getEmailGateway() == null) {
                    return true;
                } else if (!MessageUtils.isAlias(number)) {
                    return true;
                }
            }
        }
        return false;
    }

    public String formatInvalidNumbers(boolean isMms) {
        StringBuilder sb = new StringBuilder();
        for (String number : mTokenizer.getNumbers()) {
            if (!isValidAddress(number, isMms)) {
                if (sb.length() != 0) {
                    sb.append(", ");
                }
                sb.append(number);
            }
        }
        return sb.toString();
    }

    public boolean containsEmail() {
        if (TextUtils.indexOf(getText(), '@') == -1)
            return false;

        List<String> numbers = mTokenizer.getNumbers();
        for (String number : numbers) {
            if (ContactRecipientEntryUtils.isEmailAddress(number))
                return true;
        }
        return false;
    }

    public static CharSequence contactToToken(Contact c) {
        SpannableString s = new SpannableString(c.getNameAndNumber());
        int len = s.length();

        if (len == 0) {
            return s;
        }

        s.setSpan(new Annotation("number", c.getNumber()), 0, len,
                Spannable.SPAN_EXCLUSIVE_EXCLUSIVE);

        return s;
    }

    /**
     * Display recipients haven't avatar.
     * @param list
     */
    public void populate(ContactList list) {
        // Very tricky bug. In the recipient editor, we always leave a trailing
        // comma to make it easy for users to add additional recipients. When a
        // user types (or chooses from the dropdown) a new contact Mms has never
        // seen before, the contact gets the correct trailing comma. But when the
        // contact gets added to the mms's contacts table, contacts sends out an
        // onUpdate to CMA. CMA would recompute the recipients and since the
        // recipient editor was still visible, call mRecipientsEditor.populate(recipients).
        // This would replace the recipient that had a comma with a recipient
        // without a comma. When a user manually added a new comma to add another
        // recipient, this would eliminate the span inside the text. The span contains the
        // number part of "Fred Flinstone <123-1231>". Hence, the whole
        // "Fred Flinstone <123-1231>" would be considered the number of
        // the first recipient and get entered into the canonical_addresses table.
        // The fix for this particular problem is very easy. All recipients have commas.
        // TODO: However, the root problem remains. If a user enters the recipients editor
        // and deletes chars into an address chosen from the suggestions, it'll cause
        // the number annotation to get deleted and the whole address (name + number) will
        // be used as the number.
        if (list.size() == 0) {
            // The base class RecipientEditTextView will ignore empty text. That's why we need
            // this special case.
            setText(null);
        } else {
            // Clear the recipient when add contact again
            setText("");
            for (Contact c : list) {
                // Calling setText to set the recipients won't create chips,
                // but calling append() will.

                // Need to judge  whether contactToToken(c) return valid data,if it is not,
                // do not append it so that the comma can not be displayed.
                CharSequence charSequence = contactToToken(c);
                if (charSequence != null && charSequence.length() > 0) {
                    append( charSequence+ ", ");
                }
            }
        }
    }

    public boolean containsMultiContacts(Spanned sp) {
        int len = sp.length();
        List<String> list = new ArrayList<String>();
        int start = 0;
        int i = 0;
        while (i < len + 1) {
            char c;
            if ((i == len) || ((c = sp.charAt(i)) == ',') || (c == ';')) {
                if (i > start) {
                    list.add(getNumberAt(sp, start, i, getContext()));

                    int spanLen = getSpanLength(sp, start, i, getContext());
                    if (spanLen > i) {
                        i = spanLen;
                    }
                }
                i++;
                while ((i < len) && (sp.charAt(i) == ' ')) {
                    i++;
                }
                start = i;
            } else {
                i++;
            }
        }
        return list.size() > 1;
    }

    /**
     * Display recipients have avatar.
     * @param list
     */
    public void populateWithAvatorDrawable(final ContactList list){
        if (list.size() == 0) {
            // The base class RecipientEditTextView will ignore empty text. That's why we need
            // this special case.
            setText(null);
        } else {
            // Clear the recipient when add contact again
            setText("");
            for (Contact c : list) {
                // Calling setText to set the recipients won't create chips,
                // but calling append() or appendRecipientEntry will.
                appendRecipientEntry(ContactRecipientEntryUtils.createRecipientEntryByContact(c,
                        false));
            }
        }
    }

    private int pointToPosition(int x, int y) {
        // Check layout before getExtendedPaddingTop().
        // mLayout is used in getExtendedPaddingTop().
        Layout layout = getLayout();
        if (layout == null) {
            return -1;
        }

        x -= getCompoundPaddingLeft();
        y -= getExtendedPaddingTop();


        x += getScrollX();
        y += getScrollY();

        int line = layout.getLineForVertical(y);
        int off = layout.getOffsetForHorizontal(line, x);

        return off;
    }

    @Override
    public boolean onTouchEvent(MotionEvent ev) {
        final int action = ev.getAction();
        final int x = (int) ev.getX();
        final int y = (int) ev.getY();

        if (action == MotionEvent.ACTION_DOWN) {
            mLongPressedPosition = pointToPosition(x, y);
        }

        return super.onTouchEvent(ev);
    }

    @Override
    protected ContextMenuInfo getContextMenuInfo() {
        if ((mLongPressedPosition >= 0)) {
            Spanned text = getText();
            if (mLongPressedPosition <= text.length()) {
                int start = mTokenizer.findTokenStart(text, mLongPressedPosition);
                int end = mTokenizer.findTokenEnd(text, start);

                if (end != start) {
                    String number = getNumberAt(getText(), start, end, getContext());
                    Contact c = Contact.get(number, false);
                    return new RecipientContextMenuInfo(c);
                }
            }
        }
        return null;
    }

    private static String getNumberAt(Spanned sp, int start, int end, Context context) {
        String number = getFieldAt("number", sp, start, end, context);
        number = PhoneNumberUtils.replaceUnicodeDigits(number);
        if (!TextUtils.isEmpty(number)) {
            if (number != null && number.contains("@")) {
                number = number.replaceAll(" ", "");
            }
            int pos = number.indexOf('<');
            if (pos >= 0 && pos < number.indexOf('>')) {
                // The number looks like an Rfc882 address, i.e. <fred flinstone> 891-7823
                Rfc822Token[] tokens = Rfc822Tokenizer.tokenize(number);
                if (tokens.length == 0) {
                    return number;
                }
                return tokens[0].getAddress();
            }
        }
        return number;
    }

    private static int getSpanLength(Spanned sp, int start, int end, Context context) {
        // TODO: there's a situation where the span can lose its annotations:
        //   - add an auto-complete contact
        //   - add another auto-complete contact
        //   - delete that second contact and keep deleting into the first
        //   - we lose the annotation and can no longer get the span.
        // Need to fix this case because it breaks auto-complete contacts with commas in the name.
        Annotation[] a = sp.getSpans(start, end, Annotation.class);
        if (a.length > 0) {
            return sp.getSpanEnd(a[0]);
        }
        return 0;
    }

    private static String getFieldAt(String field, Spanned sp, int start, int end,
            Context context) {
        Annotation[] a = sp.getSpans(start, end, Annotation.class);
        String fieldValue = getAnnotation(a, field);
        if (TextUtils.isEmpty(fieldValue)) {
            fieldValue = TextUtils.substring(sp, start, end);
        }
        return fieldValue;

    }

    private static String getAnnotation(Annotation[] a, String key) {
        for (int i = 0; i < a.length; i++) {
            if (a[i].getKey().equals(key)) {
                return a[i].getValue();
            }
        }

        return "";
    }

    private class RecipientsEditorTokenizer
            implements MultiAutoCompleteTextView.Tokenizer {

        @Override
        public int findTokenStart(CharSequence text, int cursor) {
            int i = cursor;
            char c;

            // If we're sitting at a delimiter, back up so we find the previous token
            if (i > 0 && ((c = text.charAt(i - 1)) == ',' || c == ';')) {
                --i;
            }
            // Now back up until the start or until we find the separator of the previous token
            while (i > 0 && (c = text.charAt(i - 1)) != ',' && c != ';') {
                i--;
            }
            while (i < cursor && text.charAt(i) == ' ') {
                i++;
            }
            //filter Full width space
            while (i < cursor && text.charAt(i) == '\u3000') {
                i++;
            }

            return i;
        }

        @Override
        public int findTokenEnd(CharSequence text, int cursor) {
            int i = cursor;
            int len = text.length();
            char c;

            while (i < len) {
                if ((c = text.charAt(i)) == ',' || c == ';') {
                    return i;
                } else {
                    i++;
                }
            }

            return len;
        }

        @Override
        public CharSequence terminateToken(CharSequence text) {
            int i = text.length();

            while (i > 0 && text.charAt(i - 1) == ' ') {
                i--;
            }

            char c;
            if (i > 0 && ((c = text.charAt(i - 1)) == ',' || c == ';')) {
                return text;
            } else {
                // Use the same delimiter the user just typed.
                // This lets them have a mixture of commas and semicolons in their list.
                String separator = mLastSeparator + " ";
                if (text instanceof Spanned) {
                    SpannableString sp = new SpannableString(text + separator);
                    TextUtils.copySpansFrom((Spanned) text, 0, text.length(),
                                            Object.class, sp, 0);
                    return sp;
                } else {
                    return text + separator;
                }
            }
        }

        public List<String> getNumbers() {
            Spanned sp = RecipientsEditor.this.getText();
            int len = sp.length();
            List<String> list = new ArrayList<String>();

            int start = 0;
            int i = 0;
            while (i < len + 1) {
                char c;
                if ((i == len) || ((c = sp.charAt(i)) == ',') || (c == ';')) {
                    if (i > start) {
                        list.add(getNumberAt(sp, start, i, getContext()));

                        // calculate the recipients total length. This is so if
                        // the name contains
                        // commas or semis, we'll skip over the whole name to
                        // the next
                        // recipient, rather than parsing this single name into
                        // multiple
                        // recipients.
                        int spanLen = getSpanLength(sp, start, i, getContext());
                        if (spanLen > i) {
                            i = spanLen;
                        }
                    }

                    i++;

                    while ((i < len) && (sp.charAt(i) == ' ')) {
                        i++;
                    }

                    start = i;
                } else {
                    i++;
                }
            }
            mRecipientsCount = list.size();

            return list;
        }

        public String getNumbersString() {
            Spanned sp = RecipientsEditor.this.getText();
            int len = sp.length();
            StringBuilder sb = new StringBuilder();
            int start = 0;
            int i = 0;
            while (i < len + 1) {
                char c;
                if ((i == len) || ((c = sp.charAt(i)) == ',') || (c == ';')) {
                    if (i > start) {
                        sb.append("'" + getNumberAt(sp, start, i, mContext) + "',");
                        // calculate the recipients total length. This is so if
                        // the name contains
                        // commas or semis, we'll skip over the whole name to
                        // the next
                        // recipient, rather than parsing this single name into
                        // multiple
                        // recipients.
                        int spanLen = getSpanLength(sp, start, i, mContext);
                        if (spanLen > i) {
                            i = spanLen;
                        }
                    }

                    i++;

                    while ((i < len) && (sp.charAt(i) == ' ')) {
                        i++;
                    }

                    start = i;
                } else {
                    i++;
                }
            }

            return (sb.length() != 0) ? (sb.deleteCharAt(sb.length() - 1).toString()) : null;
        }
    }

    static class RecipientContextMenuInfo implements ContextMenuInfo {
        final Contact recipient;

        RecipientContextMenuInfo(Contact r) {
            recipient = r;
        }
    }
}
