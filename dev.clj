(ns dev
  (:import Reactor))

(def r (Reactor.))

(def reactor-ptr (.create_reactor r 16))

(def event-fd (.create_eventfd r))

(defn listener [v]
  (prn "[Clojure] Value received from eventfd: " v))

(comment

  (.listen_eventfd r reactor-ptr event-fd listener)
  (.trigger_eventfd r event-fd 1)

  (.reactor_run r reactor-ptr 1000)

  nil)
