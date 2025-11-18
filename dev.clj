(ns dev
  (:import Reactor))

(def EPOLLIN (. Reactor -EPOLLIN))

(def r (Reactor.))

(def reactor-ptr (.create_reactor r 16))

(def event-fd (.create_eventfd r))

(defn watcher-callback [fd evs]
  (let [v (.read_eventfd r fd)]
    (prn "[Clojure] Value received from eventfd: " v)))

(def watcher (.create_watcher r reactor-ptr event-fd EPOLLIN watcher-callback))

(comment

  (.inc_eventfd r event-fd)

  (.reactor_run r reactor-ptr 2000)

  nil)
