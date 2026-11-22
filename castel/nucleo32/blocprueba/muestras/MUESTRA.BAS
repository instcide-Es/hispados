10 REM un programa: FOR IF
20 FOR I% = 1 TO 10: PRINT I%: NEXT
30 IF A$ = "SI" THEN 100
40 IF X > &H1F THEN
50   PRINT "grande"; ' comentario
60 ELSEIF X = 0 THEN
70   PRINT LEFT$(A$, 2)
80 END IF
DECLARE SUB Pinta (n%)
SUB Pinta (n%)
  WHILE n% > 0: n% = n% - 1: WEND
END SUB
100 IMPRIMIR "hola": NOTA en español
110 END
