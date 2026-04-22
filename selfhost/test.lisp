(def 'seq
  (\ (n acc)
    (cond
      ((eq n 0) acc)
      (1 (seq (- n 1) (cons (- n 1) acc))))))

