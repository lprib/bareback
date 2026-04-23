(def 'seq
  (fn (n acc)
    (cond
      ((eq n 0) acc)
      (1 (seq (- n 1) (cons (- n 1) acc))))))

(def 'map
  (fn (list fn)
    (cond
      (list (cons (fn (car list)) (map (cdr list) fn)))
      (1 nil))))

(def '+1 (fn (x) (+ x 1)))
(def '-1 (fn (x) (- x 1)))

(def 'fact
  (fn (x)
      (cond
        ((eq x 0) 1)
        (1 (* x (fact (-1 x)))))))
