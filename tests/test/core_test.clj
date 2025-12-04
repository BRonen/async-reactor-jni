(ns core-test
  (:require [clojure.test :refer [deftest testing is use-fixtures]])
  (:import Reactor
           [java.io File]))

(defonce reactor (atom nil))

(defn with-reactor [f]
  (try
    (reset! reactor (Reactor/create_reactor 4))
    (f)

    (finally
      (Reactor/free_reactor @reactor))))

(def temp-file-path "temp_reactor_test_file.txt")
(def initial-content "Hello World!")

(defn setup-temp-file []
  (let [file (File. temp-file-path)]
    (spit file initial-content)
    (.getAbsolutePath file)))

(defn cleanup-temp-file []
  (.delete (File. temp-file-path)))

(defn with-temp-file [f]
  (try
    (setup-temp-file)
    (f)
    (finally (cleanup-temp-file))))

(use-fixtures :once with-reactor with-temp-file)

(deftest reactor-initialization
  (testing "Reactor instance should be available"
    (is (-> @reactor nil? not))))

(deftest reactor-filesystem-io
  (testing "File open and close should succeed"
    (let [path (setup-temp-file)
          fd   (Reactor/open path Reactor/RDONLY)]
      (is (pos-int? fd))
      (Reactor/close fd)))

  (testing "File read should complete and deliver correct data"
    (let [path       (setup-temp-file)
          fd         (Reactor/open path 0)
          buffer-len (count initial-content)
          result     (promise)
          callback   (fn [buf len]
                       (is (= len buffer-len))
                       (let [data (byte-array len)]
                         (.get buf data)
                         (deliver result (String. data "UTF-8"))))]
      (is (nil? (deref result 100 nil)))

      (Reactor/file_read @reactor fd buffer-len 0 callback)
      (is (nil? (deref result 100 nil)))

      (Reactor/reactor_run @reactor 200)
      (is (= initial-content (deref result 100 nil)))

      (Reactor/close fd))))

(deftest reactor-eventfd
  (testing "Sending a value through eventfd"
    (let [efd (Reactor/create_eventfd 0)
          result (promise)
          callback (partial deliver result)]

      (Reactor/listen_eventfd @reactor efd callback)
      (is (nil? (deref result 100 nil)))

      (Reactor/reactor_run @reactor 200)
      (is (nil? (deref result 100 nil)))

      (Reactor/trigger_eventfd efd 1)
      (is (nil? (deref result 100 nil)))

      (Reactor/reactor_run @reactor 200)
      (is (= 1 (deref result 100 nil))))))
