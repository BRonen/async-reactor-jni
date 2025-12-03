(ns core-test
  (:require [clojure.test :refer [deftest testing is]])
  (:import Reactor))

(deftest sum
  (testing "2 + 2"
    (is (= 4 (+ 2 2)))))
