var imf = function () {
	var lf = 0;
	var instances = [];
	function getElementsByClass (object, tag, className) {
		var o = object.getElementsByTagName(tag);
        var n = o.length;
        var ret = [];
		for ( var i = 0; i <n; i++) {
            if (o[i].className == className) ret.push(o[i]);
		}
		if (ret.length == 1) ret = ret[0];
		return ret;
	}
	function addEvent (o, e, f) {
		if (window.addEventListener) o.addEventListener(e, f, false);
		else if (window.attachEvent) r = o.attachEvent('on' + e, f);
	}
	function createReflexion (cont, img) {
		var flx = false;
		if (document.createElement("canvas").getContext) {
			flx = document.createElement("canvas");
			flx.width = img.width;
			flx.height = img.height;
			var context = flx.getContext("2d");
			context.translate(0, img.height);
			context.scale(1, -1);
			context.drawImage(img, 0, 0, img.width, img.height);
			context.globalCompositeOperation = "destination-out";
			var gradient = context.createLinearGradient(0, 0, 0, img.height * 2);
			gradient.addColorStop(1, "rgba(255, 255, 255, 0)");
			gradient.addColorStop(0, "rgba(255, 255, 255, 1)");
			context.fillStyle = gradient;
			context.fillRect(0, 0, img.width, img.height * 2);
		} else {
			/* ---- DXImageTransform ---- */
			flx     = document.createElement('img');
			flx.src = img.src;
			flx.style.filter = 'flipv progid:DXImageTransform.Microsoft.Alpha(' +
			                   'opacity=50, style=1, finishOpacity=0, startx=0, starty=0, finishx=0, finishy=' +
							   (img.height * .25) + ')';
		}
		/* ---- insert Reflexion ---- */
		flx.style.position = 'absolute';
		//flx.style.left     = '-1000px';
		flx.style.display  = "none";
		cont.appendChild(flx);
		return flx;
	}
	/* //////////// ==== ImageFlow Constructor ==== //////////// */
	function ImageFlow(oCont, size, zoom, border) {
		this.diapos     = [];
		this.scr        = false;
		this.size       = size;
		this.zoom       = zoom;
		this.bdw        = border;
		this.oCont      = oCont;
		this.oc         = document.getElementById(oCont);
		this.scrollbar  = getElementsByClass(this.oc,   'div', 'scrollbar');
		this.text       = getElementsByClass(this.oc,   'div', 'text');
		this.title      = getElementsByClass(this.text, 'div', 'title');
		this.legend     = getElementsByClass(this.text, 'div', 'legend');
		this.bar        = getElementsByClass(this.oc,   'img', 'bar');
		this.arL        = getElementsByClass(this.oc,   'img', 'arrow-left');
		this.arR        = getElementsByClass(this.oc,   'img', 'arrow-right');
		this.bw         = this.bar.width;
		this.alw        = this.arL.width - 5;
		this.arw        = this.arR.width - 5;
		this.bar.parent = this.oc.parent  = this;
		this.arL.parent = this.arR.parent = this;
		this.view       = this.back       = -1;
		this.resize();
		this.oc.onselectstart = function () { return false; }
		/* ---- create images ---- */
		var img   = getElementsByClass(this.oc, 'div', 'bank').getElementsByTagName('a');
		this.NF = img.length;
		for (var i = 0, o; o = img[i]; i++) {
			this.diapos[i] = new Diapo(this, i,
										o.rel,
										o.title || '- ' + i + ' -',
										o.innerHTML || o.rel,
										o.href || '',
										o.target || '_self'
			);
		}
		this.resize();
		/* ==== add mouse wheel events ==== */
		if (window.addEventListener)
			this.oc.addEventListener('DOMMouseScroll', function(e) {
				this.parent.scroll(-e.detail);
			}, false);
		else this.oc.onmousewheel = function () {
			this.parent.scroll(event.wheelDelta);
		}
		
		//this.oc.addEventListener('keyup', function(e){alert("nimamma");}, false);
	}
	/* //////////// ==== ImageFlow prototype ==== //////////// */
	ImageFlow.prototype = {
		/* ==== targets ==== */
		calc : function (inc) {
			if (inc) this.view += inc;
			var tw = 0;
			var lw = 0;
			var o = this.diapos[this.view];
			if (o && o.loaded) {
				/* ---- reset ---- */
				var ob = this.diapos[this.back];
				if (ob && ob != o) {
					ob.img.className = 'diapo';
					ob.z1 = 1;
				}
				/* ---- update legend ---- */
				this.title.replaceChild(document.createTextNode(o.title), this.title.firstChild);
				this.legend.replaceChild(document.createTextNode(o.text), this.legend.firstChild);
				/* ---- update hyperlink ---- */
				if (o.url) {
					o.img.className = 'diapo link';
					window.status = 'hyperlink: ' + o.url;
				} else {
					o.img.className = 'diapo';
					window.status = '';
				}
				/* ---- calculate target sizes & positions ---- */
				o.w1 = Math.min(o.iw, this.wh * .5) * o.z1;
				var x0 = o.x1 = (this.wh * .5) - (o.w1 * .5);
				var x = x0 + o.w1 + this.bdw;
				for (var i = this.view + 1, o; o = this.diapos[i]; i++) {
					if (o.loaded) {
						o.x1 = x;
						o.w1 = (this.ht / o.r) * this.size;
						x   += o.w1 + this.bdw;
						tw  += o.w1 + this.bdw;
					}
				}
				x = x0 - this.bdw;
				for (var i = this.view - 1, o; o = this.diapos[i]; i--) {
					if (o.loaded) {
						o.w1 = (this.ht / o.r) * this.size;
						o.x1 = x - o.w1;
						x   -= o.w1 + this.bdw;
						tw  += o.w1 + this.bdw;
						lw  += o.w1 + this.bdw;
					}
				}				
				/* ---- save preview view ---- */
				this.back = this.view;
			}
		},
		/* ==== mousewheel scrolling ==== */
		scroll : function (sc) {
			if (sc < 0) {
				if (this.view < this.NF - 1) this.calc(1);
			} else {
				if (this.view > 0) this.calc(-1);
			}
		},
		enteritem : function()
		{
			if(this.view > this.NF - 1)this.view = this.NF - 1 ;
			if(this.view < 0 )this.view = 0;
			this.diapos[this.view].click();
		},
		/* ==== resize  ==== */
		resize : function () {
			this.wh = this.oc.clientWidth;
			this.ht = this.oc.clientHeight;
			this.ws = this.scrollbar.offsetWidth;
			this.calc();
			this.run(true);
			if(this.NF)
			{
			   var i =	this.view;
			   var mid = parseInt((this.NF + 1) /2) - 1;
			   if(i < mid)
			   {
			    while(i < mid)
				{
				   this.scroll(-1);
				   i++;
				}
			   }
			   else if(i > mid)
			   {
				while(i > mid)
				{
				   this.scroll(1);
				   i--;
				}
			   }
			}
		},
		/* ==== move all images  ==== */
		run : function (res) {
			var i = this.NF;			
			while (i--) this.diapos[i].move(res);
		}
	}
	/* //////////// ==== Diapo Constructor ==== //////////// */
	Diapo = function (parent, N, src, title, text, url, target) {
		this.parent        = parent;
		this.loaded        = false;
		this.title         = title;
		this.text          = text;
		this.url           = url;
		this.target        = target;
		this.N             = N;
		this.img           = document.createElement('img');
		this.img.src       = src;
		this.img.parent    = this;
		this.img.className = 'diapo';
		this.x0            = this.parent.oc.clientWidth;
		this.x1            = this.x0;
		this.w0            = 0;
		this.w1            = 0;
		this.z1            = 1;
		this.img.parent    = this;
		this.img.onclick   = function() { this.parent.click(); }
		this.img.style.display = "none";
		this.parent.oc.appendChild(this.img);
		/* ---- display external link ---- */
		if (url) {
			this.img.onmouseover = function () { this.className = 'diapo link';	}
			this.img.onmouseout  = function () { this.className = 'diapo'; }
		}
	}
	/* //////////// ==== Diapo prototype ==== //////////// */
	Diapo.prototype = {
		/* ==== HTML rendering ==== */
		move : function (res) {
			if (this.loaded) {
				this.img.style.display = "";
				if(this.flx)this.flx.style.display = "";
				var sx = this.x1 - this.x0;
				var sw = this.w1 - this.w0;
				if (Math.abs(sx) > 2 || Math.abs(sw) > 2 || res) {
					/* ---- paint only when moving ---- */
					this.x0 += sx * .2;
					this.w0 += sw * .2;
					if (this.x0 < this.parent.wh && this.x0 + this.w0 > 0) {
						/* ---- paint only visible images ---- */
						this.visible = true;
						var o = this.img.style;
						var h = this.w0 * this.r;
						/* ---- diapo ---- */
						o.left   = Math.round(this.x0) + 'px';
						o.bottom = Math.floor(this.parent.ht * .25) + 'px';
						o.width  = Math.round(this.w0) + 'px';
						o.height = Math.round(h) + 'px';
						/* ---- reflexion ---- */
						if (this.flx) {
							var o = this.flx.style;
							o.left   = Math.round(this.x0) + 'px';
							o.top    = Math.ceil(this.parent.ht * .75 + 1) + 'px';
							o.width  = Math.round(this.w0) + 'px';
							o.height = Math.round(h) + 'px';
						}
					} else {
						/* ---- disable invisible images ---- */
						if (this.visible) {
							this.visible = false;
							this.img.style.width = '0px';
							if (this.flx) this.flx.style.width = '0px';
						}
					}
				}
			} else {
				/* ==== image onload ==== */
				if (this.img.complete && this.img.width) {
					/* ---- get size image ---- */
					this.iw     = this.img.width;
					this.ih     = this.img.height;
					this.r      = this.ih / this.iw;
					this.loaded = true;
					/* ---- create reflexion ---- */
					this.flx    = createReflexion(this.parent.oc, this.img);
					if (this.parent.view < 0) this.parent.view = this.N;
					this.parent.calc();
				}
			}
		},
		/* ==== diapo onclick ==== */
		click : function () {
			if (this.parent.view == this.N) {
				/* ---- click on zoomed diapo ---- */
				if (this.url) {
					/* ---- open hyperlink ---- */
					window.open(this.url, this.target);
				} else {}
					/* ---- zoom in/out ---- */
				this.z1 = this.z1 == 1 ? this.parent.zoom : 1;
				this.parent.calc();
				
			} else {
				/* ---- select diapo ---- */
				this.parent.view = this.N;
				this.parent.calc();
			}
			return false;
		}
	}
	/* //////////// ==== public methods ==== //////////// */
	return {
		/* ==== initialize script ==== */
		create : function (div, size, zoom, border) {
			/* ---- instanciate imageFlow ---- */
			var load = function () {
				var loaded = false;
				var i = instances.length;
				while (i--) if (instances[i].oCont == div) loaded = true;
				if (!loaded) {
					/* ---- push new imageFlow instance ---- */
					instances.push(
						new ImageFlow(div, size, zoom, border)
					);
					/* ---- init script (once) ---- */
					if (!imf.initialized) {
						imf.initialized = true;
						/* ---- window resize event ---- */
						addEvent(window, 'resize', function () {
							var i = instances.length;
							while (i--) instances[i].resize();
						});
						/* ---- stop drag N drop ---- */
						addEvent(document.getElementById(div), 'mouseout', function (e) {
							if (!e) e = window.event;
							var tg = e.relatedTarget || e.toElement;
							if (tg && tg.tagName == 'HTML') {
								var i = instances.length;
								while (i--) instances[i].oc.onmousemove = null;
							}
							return false;
						});
						/* ---- set interval loop ---- */
						setInterval(function () {
							var i = instances.length;
							while (i--) instances[i].run();
						}, 50);								
					}
				}
				
			}		
			/* ---- window onload event ---- */
			//window.onkeyup = function(){alert("jba~~~~~~~~")}			
			addEvent(window, 'load', function () { load(); });
		},
		
		goNext : function(step)
		{
			var i = instances.length - 1;
			if(instances[i])instances[i].scroll(step);	
		},
		enter : function()
		{
			var i = instances.length - 1;
			if(instances[i])instances[i].enteritem();				
		}
	}
}();

/* ==== create imageFlow ==== */
//          div ID    , size, zoom, border
imf.create("imageFlowQuene", 0.15, 1, 80);
