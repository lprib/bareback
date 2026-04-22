(def 'seq
  (\ (n)
    (cond
      ((eq n 0) (cons 0 nil))
      (1 (cons n (seq (- n 1)))))))

