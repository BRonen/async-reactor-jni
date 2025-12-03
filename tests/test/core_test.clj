(ns core-test
  (:require [clojure.test :refer [deftest testing is use-fixtures]])
  (:import Reactor))

(defonce reactor (atom nil))

(defn with-reactor [f]
  (reset! reactor (Reactor/create_reactor 4))
  (f)
  (Reactor/free_reactor @reactor))

(use-fixtures :once with-reactor)

(deftest reactor-initialization
  (testing "Reactor instance should be available"
    (is (-> @reactor nil? not))))

(deftest reactor-eventfd
  (testing "Sending a value through eventfd"
    (let [efd (Reactor/create_eventfd 0)
          result (promise)
          callback (partial deliver result)]

      (Reactor/listen_eventfd @reactor efd callback)
      (is (nil? (deref result 100 nil)))

      (Reactor/reactor_run @reactor 100)
      (is (nil? (deref result 100 nil)))

      (Reactor/trigger_eventfd efd 1)
      (is (nil? (deref result 100 nil)))

      (Reactor/reactor_run @reactor 100)
      (is (= 1 (deref result 100 nil))))))
