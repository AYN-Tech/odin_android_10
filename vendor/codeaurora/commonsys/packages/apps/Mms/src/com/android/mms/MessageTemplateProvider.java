/*
 *
 * Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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

package com.android.mms;

import android.util.Xml;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.SAXException;
import org.xmlpull.v1.XmlSerializer;
import android.content.Context;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.StringWriter;
import java.util.ArrayList;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.UriMatcher;
import android.database.Cursor;
import android.database.CursorWindow;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;

/**
 * Template Message provider.
 */
public class MessageTemplateProvider extends android.content.ContentProvider {

    final static String TAG = "MessageTemplateProvider";
    final static String AUTHORITY = "com.android.mms.MessageTemplateProvider";
    final static String MESSAGFILE = "message_template.xml";
    private static final boolean DBG = true;
    private String mLocale = "";
    private static final int                MESSAGES                = 1;
    private static final int                MESSAGE_ID              = 2;
    private static final UriMatcher         sUriMatcher;
    private int preDefineNum;  // numbers of the pre-define template messages.

    static
    {
        sUriMatcher = new UriMatcher(UriMatcher.NO_MATCH);
        sUriMatcher.addURI(AUTHORITY, "messages", MESSAGES);
        sUriMatcher.addURI(AUTHORITY, "messages/#", MESSAGE_ID);
    }

    public MessageTemplateProvider() {
        super();
    }

    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) {

        if (sUriMatcher.match(uri)!= MESSAGE_ID) {
            if (DBG)
                Log.d(TAG, "delete: Uri:"+uri.toString()+" not match!");
            return -1;
        }
        String _id = uri.getPathSegments().get(1);
        int id = Integer.parseInt(_id);
        Log.d("Emmet", id+"");
        Document doc = getXMLDoc();
        if (doc == null) {
            if (DBG)
                Log.e(TAG, "delete: Get XML Document failded!");
            return -1;
        }
        Element root = doc.getDocumentElement();

        NodeList lists = root.getElementsByTagName("message");
        if (id >= lists.getLength() || id < 0 ) {
            if (DBG)
                Log.e(TAG, "delete: id="+id+" nodelist length="+lists.getLength());
            return -1;
        }
        if (id < preDefineNum) {
            NodeList preDefineNumStateLists =
                        root.getElementsByTagName("preDefineNumState");
            for (int i = id; i < preDefineNum - 1; i ++) {
                Node nodeState = preDefineNumStateLists.item(i);
                Node nextNodeState = preDefineNumStateLists.item(i + 1);
                nodeState.setTextContent(nextNodeState.getTextContent().toString());
            }
            Node node = preDefineNumStateLists.item(preDefineNum - 1);
            node.setTextContent("modify");
        }
        root.removeChild(lists.item(id));
        if (!saveXMLDoc(doc)) {
            if (DBG)
                Log.e(TAG,"delete: save xml file error!");
            return -1;
        }
        return 0;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {

        if (sUriMatcher.match(uri)!= MESSAGES) {
            if (DBG)
                Log.d(TAG, "insert: Uri:"+uri.toString()+" not match!");
            return null;
        }

        if (values.containsKey("message") == false ) {
            if (DBG)
                Log.d(TAG, "insert: values not valid");
            return null;
        }

        Document doc  = getXMLDoc();
        if (doc == null) {
            if (DBG)
                Log.e(TAG, "insert: Get XML Document failded!");
            return null;
        }
        Element root = doc.getDocumentElement();
        Element newChild = doc.createElement("message");
        newChild.setTextContent(values.getAsString("message"));
        root.appendChild(newChild);

        if (!saveXMLDoc(doc)) {
            if (DBG)
                Log.e(TAG, "Save XML Document failded!");
            return null;
        }
        return uri;
    }

    @Override
    public boolean onCreate() {
        preDefineNum = ((String[])(getContext().getResources()
                .getStringArray(R.array.pre_define_message_template))).length;
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection,
            String[] selectionArgs, String sortOrder) {

        if (sUriMatcher.match(uri)!= MESSAGES) {
            if (DBG)
                Log.d(TAG, "query: Uri:"+uri.toString()+" not match!");
            return null;
        }

        Document doc = getXMLDoc();
        if (doc == null) {
            if (DBG)
                Log.d(TAG, "query: get XML documnet failed!");
            return null;
        }

        Element root = doc.getDocumentElement();

        String [] columnNames = {"_id", "message"};
        MatrixCursor cursor = new MatrixCursor(columnNames);

        NodeList lists = root.getElementsByTagName("message");
        for (int i = 0; i < lists.getLength(); i++) {
            Node node = lists.item(i);
            Object[] obj = {i,node.getTextContent()};
            cursor.addRow(obj);
        }
        return cursor;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) {

        if (sUriMatcher.match(uri)!= MESSAGES) {
            if (DBG)
                Log.d(TAG, "update: Uri:"+uri.toString()+" not match!");
            return -1;
        }

        if (values.containsKey("_id") == false || values.containsKey("message") == false ) {
            if (DBG)
                Log.d(TAG, "update: values not valid");
            return -1;
        }

         Document doc = getXMLDoc();
         if (doc == null) {
            if (DBG)
                Log.d(TAG, "update: get XML documnet failed!");
            return -1;
         }

         Element root = doc.getDocumentElement();
         NodeList lists = root.getElementsByTagName("message");
         int id = values.getAsInteger("_id");
         String message = values.getAsString("message");

         if (id < 0 || id >= lists.getLength()) {
             if (DBG)
                 Log.d(TAG, "update: id="+id+" nodelist length="+lists.getLength());
             return -1;
         }
         Node node = lists.item(id);
         if (id < preDefineNum) {
             NodeList preDefineNumStateLists =
                     root.getElementsByTagName("preDefineNumState");
             Node nodeState = preDefineNumStateLists.item(id);
             if (!node.getTextContent().toString().equals(message)) {
                 nodeState.setTextContent("modify");
             }
         }

         node.setTextContent(message);
         if (!saveXMLDoc(doc)) {
             if (DBG)
                 Log.e(TAG, "update: save XML file failed!");
             return -1;
         }
         return 0;
    }

    private void updateXML() {
        String[] smsTempPreArray = getContext().getResources().getStringArray(
                    R.array.pre_define_message_template);
        Document doc = getXMLDoc();
        if (doc == null) {
            if (DBG)
                Log.d(TAG, "query: get XML documnet failed!");
            return ;
        }
        Element root = doc.getDocumentElement();
        NodeList lists = root.getElementsByTagName("message");
        NodeList preDefineNumStateLists =
                 root.getElementsByTagName("preDefineNumState");
        int listLength = lists.getLength();
        for (int i = 0; i < preDefineNum && i < listLength; i++) {
            lists = root.getElementsByTagName("message");
            Node nodeState = preDefineNumStateLists.item(i);
            if(!(nodeState.getTextContent().toString().equals("modify"))) {
                Node node = lists.item(i);
                int index =  Integer.parseInt(nodeState.getTextContent().toString());
                node.setTextContent(smsTempPreArray[index]);
            }
            if (!saveXMLDoc(doc)) {
                if (DBG)
                    Log.e(TAG, "update: save XML file failed!");
            }
        }
    }

    Document getXMLDoc() {

        DocumentBuilderFactory docBuilderFactory = null;
        DocumentBuilder docBuilder = null;
        Document doc = null;

        docBuilderFactory = DocumentBuilderFactory.newInstance();
        try {
            docBuilder = docBuilderFactory.newDocumentBuilder();
        } catch (ParserConfigurationException e2) {
            // TODO Auto-generated catch block
            if (DBG)
                Log.e(TAG, "Generate DOM builder failed!");
            return null;
        }

        File messageFile = new File(getContext().getFilesDir(), MESSAGFILE) ;
        String locale = getContext().getResources().getConfiguration().locale.toString();
        /*
         * When we change system language, current language is different from last language.
         * In this case, we also need to call CreateMessageTemplateXML().
         */
        if (!messageFile.exists()) {
            Log.d(TAG, "init template file at: " + messageFile.getAbsolutePath());
            if (!CreateMessageTemplateXML(messageFile))
                return null;
        }

        if (!mLocale.equals(locale)) {
            // Keep mLocale consistent with current locale.
            mLocale = locale;
            updateXML();
        }

        FileInputStream inputStream = null;
        try {
            inputStream = new FileInputStream(messageFile);
        } catch (FileNotFoundException e1) {
            // TODO Auto-generated catch block
            if (DBG)
                Log.e(TAG, "Open message template file exception!");
            e1.printStackTrace();
            return null;
        }

        try {
            doc = docBuilder.parse(inputStream);
        } catch (SAXException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return null;
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            return null;
        }
        if (doc == null) {
            if (DBG)
                Log.d(TAG, "Cannot parse the message template file!");
            return null;
        }
        return doc;
    }

    boolean saveXMLDoc(Document doc) {
        TransformerFactory transFactory = TransformerFactory.newInstance();
        Transformer transFormer = null;
        try {
            transFormer = transFactory.newTransformer();
        } catch (TransformerConfigurationException e) {
            // TODO Auto-generated catch block
            if (DBG)
                Log.e(TAG,"Generate transFormer failed!");
            return false;
        }

        DOMSource domSource = new DOMSource(doc);

        FileOutputStream out = null;
        try {
            out = getContext().openFileOutput(MESSAGFILE,
                    Context.MODE_PRIVATE);
        } catch (FileNotFoundException e) {
            // TODO Auto-generated catch block
            if (DBG)
                Log.e(TAG,"Open message template file failed!");
            return false;
        }

        StreamResult xmlResult = new StreamResult(out);
        try {
            transFormer.transform(domSource, xmlResult);
       } catch (TransformerException e) {
        // TODO Auto-generated catch block
           if (DBG)
                Log.e(TAG,"Transform XML File failed!");
            return false;
       }
        return true;
    }

    boolean CreateMessageTemplateXML(File file) {
        XmlSerializer serializer = Xml.newSerializer();
        StringWriter writer = new StringWriter();

        try {
            serializer.setOutput(writer);

            serializer.startDocument("UTF-8",true);

            serializer.startTag("","MessageTemplate");

            // load the pre-define message template
            String[] smsTempPreArray = getContext().getResources().getStringArray(
                    R.array.pre_define_message_template);

            for (int i = 0; i < smsTempPreArray.length; i++) {
                 serializer.startTag("","message");
                 serializer.text(smsTempPreArray[i]);
                 serializer.endTag("","message");
            }

            for (int i = 0; i < preDefineNum; i++) {
                 serializer.startTag("", "preDefineNumState");
                 serializer.text(Integer.toString(i));
                 serializer.endTag("", "preDefineNumState");
            }

            serializer.endTag("","MessageTemplate");
            serializer.endDocument();
        } catch(Exception e) {
            if (DBG)
                Log.d(TAG,"CreateMessageTemplateXML occurs exception!");
            return false;
        }

        FileOutputStream stream = null;
        try {
            stream = new FileOutputStream(file);
        } catch (FileNotFoundException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            if (DBG)
                Log.d(TAG,"CreateMessageTemplateXML occurs exception!");
            return false;
        }

        try {
            stream.write(writer.toString().getBytes());
            stream.close();
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            if (DBG)
                Log.d(TAG,"CreateMessageTemplateXML occurs exception!");
            return false;
        }
         return true;
    }
}
