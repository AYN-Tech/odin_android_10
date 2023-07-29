package com.android.cts.verifier;

import com.android.compatibility.common.util.TestResultHistory;

import java.io.Serializable;
import java.util.AbstractMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class TestResultHistoryCollection implements Serializable {

    private final Set<TestResultHistory> mHistoryCollection = new HashSet<>();

    /**
     * Covert object to set.
     *
     * @return A set of test result history.
     */
    public Set<TestResultHistory> asSet() {
        return mHistoryCollection;
    }

    /**
     * Add a test result history with test name, start time and end time.
     *
     * @param test a string of test name.
     * @param start start time of a test.
     * @param end end time of a test.
     */
    public void add(String test, long start, long end) {
        Set<Map.Entry> duration = new HashSet<>();
        duration.add(new AbstractMap.SimpleEntry<>(start, end));
        mHistoryCollection.add(new TestResultHistory(test, duration));
    }

    /**
     * Add test result histories for tests containing test name and a set of execution time.
     *
     * @param test test name.
     * @param durations set of start and end time.
     */
    public void addAll(String test, Set<Map.Entry> durations) {
        TestResultHistory history = new TestResultHistory(test, durations);
        boolean match = false;
        for (TestResultHistory resultHistory: mHistoryCollection) {
            if (resultHistory.getTestName().equals(test)) {
                resultHistory.getDurations().addAll(durations);
                match = true;
                break;
            }
        }
        if (match == false) {
            mHistoryCollection.add(history);
        }
    }

    /**
     * Merge test with its sub-tests result histories.
     *
     * @param prefix optional test name prefix to apply.
     * @param resultHistoryCollection a set of test result histories.
     */
    public void merge(String prefix, TestResultHistoryCollection resultHistoryCollection) {
       if (resultHistoryCollection != null) {
            resultHistoryCollection.asSet().forEach(t-> addAll(
                prefix != null ? prefix + ":" + t.getTestName() : t.getTestName(), t.getDurations()));
       }
    }

    /**
     * Merge test with its sub-tests result histories.
     *
     * @param prefix optional test name prefix to apply.
     * @param resultHistories a list of test result history collection.
     */
    public void merge(String prefix, List<TestResultHistoryCollection> resultHistories) {
        resultHistories.forEach(resultHistoryCollection -> merge(prefix, resultHistoryCollection));
    }
}
