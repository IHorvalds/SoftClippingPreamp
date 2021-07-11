# SoftClippingPreamp
First solo implementation using JUCE. 

It's using a simple soft clipping function
```y[n] = (2/pi) * arctan(gain * x[n]) + x[n]```

and an first order HPF at around 350Hz. Built starting from (this article)[https://web.archive.org/web/20181023063913/http://www.bteaudio.com/articles/TSS/TSS.html],
but changed the cutoff frequency of the filter and the maximum gain value.

Also added a tone stack built according to (this paper by David T. Yeh and Julius O. Smith) [https://ccrma.stanford.edu/~dtyeh/papers/yeh06_dafx.pdf]
and a convolution filter with a guitar cabinet impulse reponse. Not sure I can share the wav file, but use other IR loaders.


*Massive issues with aliasing.*
*High shelf parameters aren't connected to anything. The filter wasn't stable.*
