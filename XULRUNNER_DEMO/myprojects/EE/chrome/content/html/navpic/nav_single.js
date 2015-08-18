var initialized = false;
var lf = 0;
var instances = null;
var isImgFullScreen = false;
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
function createRef(img)
{
    var flx = document.createElement("canvas");
	flx.width = img.width;
	flx.height = img.height;
	var context = flx.getContext("2d");
	context.translate(0, img.height);
	context.scale(1, -1);
	try
	{
		context.drawImage(img, 0, 0, img.width, img.height);
	}
	catch(e)
	{
		return null;
	}
	context.globalCompositeOperation = "destination-out";
	var gradient = context.createLinearGradient(0, 0, 0, img.height * 2);
	gradient.addColorStop(1, "rgba(255, 255, 255, 0)");
	gradient.addColorStop(0, "rgba(255, 255, 255, 1)");
	context.fillStyle = gradient;
	context.fillRect(0, 0, img.width, img.height * 2);
	
	/* ---- insert Reflexion ---- */
	flx.style.position = 'absolute';
	flx.style.left     = '-1000px';
	flx.id = "my_flx";
	flx.style.display = "none";
    
    return flx;
}

function createReflexion (cont, srcflx)
{
	if(!srcflx)
	return null;
	var oldflx = document.getElementById("my_flx");
	if(oldflx)
	{
		cont.replaceChild(srcflx, oldflx);
	}
	else
		cont.appendChild(srcflx);
	return srcflx;
}
/* //////////// ==== ImageFlow Constructor ==== //////////// */
function ImageFlow(oCont, size, zoom, border, srcimg, srcflx) {
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
	this.title.replaceChild(document.createTextNode('Loading...'), this.title.firstChild);
	this.legend.replaceChild(document.createTextNode('Please Wait.'), this.legend.firstChild);
	/* ---- create images ---- */
	var img   = document.createElement('a');
	//img.rel = imgsrc;//
	img.title = "Show";
	this.NF = 1;
	o= img;
	//one image
	this.diapos[0] = new Diapo(this, 0,
								srcimg,
								o.title,
								"img",
								'',
								o.target || '_self',
                                srcflx
	);

	/* ==== add mouse wheel events ==== */
	if (window.addEventListener)
		this.oc.addEventListener('DOMMouseScroll', function(e) {
			this.parent.scroll(-e.detail);
		}, false);
	else this.oc.onmousewheel = function () {
		this.parent.scroll(event.wheelDelta);
	}
	/* ==== scrollbar drag N drop ==== */
	this.bar.onmousedown = function (e) {
		if (!e) e = window.event;
		var scl = e.screenX - this.offsetLeft;
		var self = this.parent;
		/* ---- move bar ---- */
		this.parent.oc.onmousemove = function (e) {
			if (!e) e = window.event;
			self.bar.style.left = Math.round(Math.min((self.ws - self.arw - self.bw), Math.max(self.alw, e.screenX - scl))) + 'px';
			self.view = Math.round(((e.screenX - scl) ) / (self.ws - self.alw - self.arw - self.bw) * self.NF);
			if (self.view != self.back) self.calc();
			return false;
		}
		/* ---- release scrollbar ---- */
		this.parent.oc.onmouseup = function (e) {
			self.oc.onmousemove = null;
			return false;
		}
		return false;
	}
	/* ==== right arrow ==== */
	this.arR.onclick = this.arR.ondblclick = function () {
		if (this.parent.view < this.parent.NF - 1)
			this.parent.calc(1);
	}
	/* ==== Left arrow ==== */
	this.arL.onclick = this.arL.ondblclick = function () {
		if (this.parent.view > 0)
			this.parent.calc(-1);
	}
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
			/* ---- move scrollbar ---- */
			if (!this.scr && tw) {
				var r = (this.ws - this.alw - this.arw - this.bw) / tw;
				this.bar.style.left = Math.round(this.alw + lw * r) + 'px';
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
	/* ==== resize  ==== */
	resize : function () {
		this.wh = this.oc.clientWidth;
		this.ht = this.oc.clientHeight;
		this.ws = this.scrollbar.offsetWidth;
		this.calc();
		this.run(true);
	},
	/* ==== move all images  ==== */
	run : function (res) {
		//var i = this.NF;
		//while (i--)
		if(this.diapos[0])this.diapos[0].move(res);
	}
}
/* //////////// ==== Diapo Constructor ==== //////////// */
Diapo = function (parent, N, srcimg, title, text, url, target, srcflx) {
	this.parent        = parent;
	this.loaded        = false;
	this.title         = title;
	this.text          = text;
	this.url           = url;
	this.target        = target;
	this.N             = N;
	this.img           = srcimg;
    this.flx           = srcflx;
	//this.img.src       = src;
	this.img.parent    = this;
	this.img.className = 'diapo';
	this.x0            = this.parent.oc.clientWidth;
	this.x1            = this.x0;
	this.w0            = 0;
	this.w1            = 0;
	this.z1            = 1;
	this.img.parent    = this;
	this.img.onclick   = function() { this.parent.click(); }
	this.img.id        = "my_picture";
	var oldflx = document.getElementById("my_flx");
	if(oldflx)
		this.parent.oc.removeChild(oldflx);
	var oldimg = document.getElementById("my_picture");
	this.img.style.display = "none";
	if(oldimg)
		this.parent.oc.replaceChild(this.img, oldimg);
	else
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
			
			if(!isImgFullScreen)
			{
				var o = this.img.style;
				var imgW = this.img.width;
				var imgH = this.img.height;
				var clientW = parseInt(this.parent.oc.clientWidth);
				var clientH = parseInt(this.parent.oc.clientHeight);
				var clientT = parseInt(this.parent.oc.clientTop);
				var clientL = parseInt(this.parent.oc.clientLeft);
				//alert(clientT + "T:L" + clientL);
				
				var _h, _w;
				if(imgW < imgH)
				{
					_h = imgH < clientH ? imgH : clientH * 0.95;
					_w = imgW / imgH * _h;
					//clientH = imgH /imgW * clientW;
				}
				else
				{
					_w = imgW < clientW ? imgW : clientW * 0.95;
					_h = imgH / imgW * _w;
				}
				o.left   = clientL + (clientW - _w)/2 + 'px';
				o.bottom = (clientH - _h)/2 + 'px';
				o.width  = _w + 'px';
				o.height = _h + 'px';
				return;
			}
			
			if (Math.abs(sx) > 2 || Math.abs(sw) > 2 || res) {
				/* ---- paint only when moving ---- */
				this.x0 += sx * .5;
				this.w0 += sw * .5;
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
				this.flx    = createReflexion(this.parent.oc, this.flx);
				if (this.parent.view < 0) this.parent.view = this.N;
				this.parent.calc();
			}
		}
	},
	/* ==== diapo onclick ==== */
	click : function () {
		if (this.parent.view == this.N) {
			/* ---- click on zoomed diapo ---- */
			//if (this.url) {
				/* ---- open hyperlink ---- */
			//	window.open(this.url, this.target);
			//} else {}
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
/* ==== initialize script ==== */
function createEx(div, size, zoom, border, srcimg, srcflx) {
	/* ---- push new imageFlow instance ---- */
	instances = new ImageFlow(div, size, zoom, border, srcimg, srcflx);
	
	/* ---- init script (once) ---- */
	if (!initialized) {
		initialized = true;
		/* ---- window resize event ---- */
		addEvent(window, 'resize', function () {
			instances.resize();
		});
		/* ---- stop drag N drop ---- */
		addEvent(document.getElementById(div), 'mouseout', function (e) {
			if (!e) e = window.event;
			var tg = e.relatedTarget || e.toElement;
			if (tg && tg.tagName == 'HTML') {
				instances.oc.onmousemove = null;
			}
			return false;
		});
		/* ---- set interval loop ---- */
		setInterval(function () {
			instances.run();
		}, 45);
	}
}

function create(div, size, zoom, border, srcimg) {
	/* ---- push new imageFlow instance ---- */
	instances = new ImageFlow(div, size, zoom, border, srcimg, 0);
	
	/* ---- init script (once) ---- */
	if (!initialized) {
		initialized = true;
		/* ---- window resize event ---- */
		addEvent(window, 'resize', function () {
			instances.resize();
		});
		/* ---- stop drag N drop ---- */
		addEvent(document.getElementById(div), 'mouseout', function (e) {
			if (!e) e = window.event;
			var tg = e.relatedTarget || e.toElement;
			if (tg && tg.tagName == 'HTML') {
				instances.oc.onmousemove = null;
			}
			return false;
		});
		/* ---- set interval loop ---- */
		setInterval(function () {
			instances.run();
		}, 45);
	}
}

function waiting(div)
{
    var oc     = document.getElementById(div);
    
    var text       = getElementsByClass(oc,   'div', 'text');
    var title      = getElementsByClass(text, 'div', 'title');
	var legend     = getElementsByClass(text, 'div', 'legend');	
	title.replaceChild(document.createTextNode('Loading...'), title.firstChild);
	legend.replaceChild(document.createTextNode('Please Wait..'), legend.firstChild);    
   
    var oldflx = document.getElementById("my_flx");
    if(oldflx) oc.removeChild(oldflx);
    var oldimg = document.getElementById("my_picture");
    var gif = document.createElement("img");
    gif.src = "img/loading.gif";
    gif.className = "loading";
	gif.id        = "my_picture";
    if(oldimg)
    {        
        oc.replaceChild(gif,oldimg);
    }
    else
        oc.appendChild(gif);
}