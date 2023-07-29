/*
 * Copyright (C) 2007-2008 Esmertec AG.
 * Copyright (C) 2007-2008 The Android Open Source Project
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

package com.android.mms.transaction;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.Uri;
import com.android.mms.MmsConfig;
import com.android.mms.ui.MessageUtils;
import com.android.mms.util.SendingProgressTokenManager;
import com.android.mmswrapper.TelephonyManagerWrapper;
import com.google.android.mms.MmsException;

/**
 * Transaction is an abstract class for notification transaction, send transaction
 * and other transactions described in MMS spec.
 * It provides the interfaces of them and some common methods for them.
 */
public abstract class Transaction extends Observable {
    private final int mServiceId;

    protected Context mContext;
    protected String mId;
    protected TransactionState mTransactionState;
    protected TransactionSettings mTransactionSettings;
    protected int mSubId;
    protected int mFailReason;

    /**
     * Identifies push requests.
     */
    public static final int NOTIFICATION_TRANSACTION = 0;
    /**
     * Identifies deferred retrieve requests.
     */
    public static final int RETRIEVE_TRANSACTION     = 1;
    /**
     * Identifies send multimedia message requests.
     */
    public static final int SEND_TRANSACTION         = 2;
    /**
     * Identifies send read report requests.
     */
    public static final int READREC_TRANSACTION      = 3;


    public static final int FAIL_REASON_NONE = 0;
    public static final int FAIL_REASON_CAN_NOT_SETUP_DATA_CALL = 1;

    public Transaction(Context context, int serviceId,
            TransactionSettings settings) {
        mContext = context;
        mTransactionState = new TransactionState();
        mServiceId = serviceId;
        mTransactionSettings = settings;
        mFailReason = FAIL_REASON_NONE;
    }

    /**
     * Returns the transaction state of this transaction.
     *
     * @return Current state of the Transaction.
     */
    @Override
    public TransactionState getState() {
        return mTransactionState;
    }

    /**
     * An instance of Transaction encapsulates the actions required
     * during a MMS Client transaction.
     */
    public abstract void process();

    /**
     * Used to determine whether a transaction is equivalent to this instance.
     *
     * @param transaction the transaction which is compared to this instance.
     * @return true if transaction is equivalent to this instance, false otherwise.
     */
    public boolean isEquivalent(Transaction transaction) {
        return mId.equals(transaction.mId);
    }

    /**
     * Get the service-id of this transaction which was assigned by the framework.
     * @return the service-id of the transaction
     */
    public int getServiceId() {
        return mServiceId;
    }

    public int getSubId() {
        return mSubId;
    }

    public void setSubId(int subId) {
        mSubId = subId;
    }

    public TransactionSettings getConnectionSettings() {
        return mTransactionSettings;
    }
    public void setConnectionSettings(TransactionSettings settings) {
        mTransactionSettings = settings;
    }

    public int getFailReason() {
        return mFailReason;
    }

    /**
     * A common method to send a PDU to MMSC.
     *
     * @param pdu A byte array which contains the data of the PDU.
     * @return A byte array which contains the response data.
     *         If an HTTP error code is returned, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(>=400) returned from the server.
     * @throws MmsException if pdu is null.
     */
    protected byte[] sendPdu(byte[] pdu) throws IOException, MmsException {
        return sendPdu(SendingProgressTokenManager.NO_TOKEN, pdu,
                mTransactionSettings.getMmscUrl());
    }

    /**
     * A common method to send a PDU to MMSC.
     *
     * @param pdu A byte array which contains the data of the PDU.
     * @param mmscUrl Url of the recipient MMSC.
     * @return A byte array which contains the response data.
     *         If an HTTP error code is returned, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(>=400) returned from the server.
     * @throws MmsException if pdu is null.
     */
    protected byte[] sendPdu(byte[] pdu, String mmscUrl) throws IOException, MmsException {
        return sendPdu(SendingProgressTokenManager.NO_TOKEN, pdu, mmscUrl);
    }

    /**
     * A common method to send a PDU to MMSC.
     *
     * @param token The token to identify the sending progress.
     * @param pdu A byte array which contains the data of the PDU.
     * @return A byte array which contains the response data.
     *         If an HTTP error code is returned, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(>=400) returned from the server.
     * @throws MmsException if pdu is null.
     */
    protected byte[] sendPdu(long token, byte[] pdu) throws IOException, MmsException {
        return sendPdu(token, pdu, mTransactionSettings.getMmscUrl());
    }

    /**
     * A common method to send a PDU to MMSC.
     *
     * @param token The token to identify the sending progress.
     * @param pdu A byte array which contains the data of the PDU.
     * @param mmscUrl Url of the recipient MMSC.
     * @return A byte array which contains the response data.
     *         If an HTTP error code is returned, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(>=400) returned from the server.
     * @throws MmsException if pdu is null.
     */
    protected byte[] sendPdu(long token, byte[] pdu,
            String mmscUrl) throws IOException, MmsException {
        if (pdu == null) {
            throw new MmsException();
        }
        setSocketTimeOut();
        return HttpUtils.httpConnection(
                mContext, token,
                mmscUrl,
                pdu, HttpUtils.HTTP_POST_METHOD,
                mTransactionSettings.isProxySet(),
                mTransactionSettings.getProxyAddress(),
                mTransactionSettings.getProxyPort(), mSubId);
    }

    /**
     * A common method to retrieve a PDU from MMSC.
     *
     * @param url The URL of the message which we are going to retrieve.
     * @return A byte array which contains the data of the PDU.
     *         If the status code is not correct, an IOException will be thrown.
     * @throws IOException if any error occurred on network interface or
     *         an HTTP error code(>=400) returned from the server.
     */
    protected byte[] getPdu(String url) throws IOException {
        setSocketTimeOut();
        return HttpUtils.httpConnection(
                mContext, SendingProgressTokenManager.NO_TOKEN,
                url, null, HttpUtils.HTTP_GET_METHOD,
                mTransactionSettings.isProxySet(),
                mTransactionSettings.getProxyAddress(),
                mTransactionSettings.getProxyPort(),mSubId);
    }

    private void setSocketTimeOut() {
        if (TelephonyManagerWrapper.is1xNetwork(mContext, mSubId)) {
            MmsConfig.setHttpSocketTimeout(60*1000*3);
        } else {
            MmsConfig.setHttpSocketTimeout(60*1000);
        }
    }

    public void cancelTransaction(Uri uri) {
    }

    public abstract void abort();

    @Override
    public String toString() {
        return getClass().getName() + ": serviceId=" + mServiceId + " subId= " + mSubId;
    }

    /**
     * Get the type of the transaction.
     *
     * @return Transaction type in integer.
     */
    abstract public int getType();
}
