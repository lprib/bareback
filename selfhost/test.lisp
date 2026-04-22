(def 'seq
  (\ (n acc)
    (cond
      ((eq n 0) acc)
      (1 (seq (- n 1) (cons (- n 1) acc))))))

(print (seq 4 nil))

(def 'map
  (\ (list fn)
    (cond
      (list (cons (fn (car list)) (map (cdr list) fn)))
      (1 nil))))
