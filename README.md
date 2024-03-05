##SRW lock: MS built-in (SRW) vs custom (PUSH) impementation

i do 4 tests with 0x10000 iterrations
8 threads ( i use 8 core cpu) randomly try acquire lock in shared (50%) and exclusive (50%) mode
if lock asquired in exclusive mode i in 50% just release it and in 50% transorm to shared first, before release
inside lock i do some very basic tests (are lock exclusive or shared acquired) and collect stat - how many shared threads at once was inside lock
after all itterations done - i show basic statistic - how many time take test and how many time N threads was inside lock at once (in shared mode of course)

in another test i use 7/1 readers vs writers and 3/1 release exclusive vs convert to shared

what was very unusual - my implementation is even have bit better perfomance compare microsoft. but may be my test not enough relevant and somebody can propose better ?


- pushlock.cpp - here all logic of custom lock is implemented. 
code used only 3 winapi calls GetCurrentThreadId, NtAlertThreadByThreadId, NtWaitForAlertByThreadId
- SrwTest.cpp - here implemented test logic
- LockUtil.cpp - for switch between SRW/Push implementation - for use same code for both tests - i only swap import function pointers to own api and back
of course last 2 files very windows specific


readers/writers 50/50

```
---------------------------
[ SRW ]
---------------------------
time = 1860

08: 00000000
07: 00000003
06: 00000062
05: 00000809
04: 00005529
03: 00022546
02: 00062284
01: 00302511


---------------------------
[ Push ]
---------------------------
time = 1141

08: 00001485
07: 00003120
06: 00006207
05: 00012537
04: 00024806
03: 00049268
02: 00096189
01: 00199536


---------------------------
[ Push ]
---------------------------
time = 1156

08: 00001540
07: 00003158
06: 00006203
05: 00012225
04: 00024346
03: 00049038
02: 00096477
01: 00200139


---------------------------
[ SRW ]
---------------------------
time = 1844

08: 00000000
07: 00000003
06: 00000064
05: 00000806
04: 00005379
03: 00022463
02: 00061698
01: 00302862
```

readers/writers 7/1
```
---------------------------
[ SRW ]
---------------------------
time = 937

08: 00000076
07: 00001059
06: 00006655
05: 00024981
04: 00059601
03: 00094590
02: 00109438
01: 00178689

---------------------------
[ Push ]
---------------------------
time = 640

08: 00017843
07: 00038308
06: 00052835
05: 00065732
04: 00079165
03: 00082202
02: 00072758
01: 00066347

---------------------------
[ Push ]
---------------------------
time = 640

08: 00017881
07: 00038189
06: 00052687
05: 00065772
04: 00079170
03: 00081549
02: 00072547
01: 00066927


---------------------------
[ SRW ]
---------------------------
time = 922

08: 00000067
07: 00000875
06: 00005824
05: 00023854
04: 00059181
03: 00095385
02: 00111268
01: 00178745
```
