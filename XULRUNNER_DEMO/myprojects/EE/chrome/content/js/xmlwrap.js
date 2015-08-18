/* Unique Interface for manipulate XMLDocument in FireFox or IE
   Created by Zoominla, 11-27-2008         
   
   @Function: Extend firefox XMLDocumnet object's function, let firefox
              has the same XMLDocument interface. Then we can manipulate
              XML in the same way.
              
   @Usage: 
    var xmlDom = new XMLDom();
	var xmlStr='<Users><User age="24">Jim</User><User age="23">Tom</User></Users>');
	xmlDom.loadXML(xmlStr);
	var Node = xmlDom.getElementsByTagName("User");
	for(var i = 0;i<Node.length;i++) {
		//alert(xml.GetNodeText(Node[i]));
		 alert(Node[i].getAttribute("age"));
	}
	var s = xmlDom.selectSingleNode("//Users/User");
	alert(GetNodeText(s));
	Node = xmlDom.selectNodes("//Users/User");
	for(var i=0;i<Node.length;i++) {
	   alert(Node[i].getAttribute("age"));
	}
*/
   
if(typeof(XML_WRAPPER_INCLUDE) != "boolean") {
	XML_WRAPPER_INCLUDE = true;
    
    var XMLDom = function()
    {
        var xmlDom = false;
        if (window.ActiveXObject)//IE
        {
            var arrSignatures = ["MSXML2.DOMDocument.5.0", "MSXML2.DOMDocument.4.0",
            "MSXML2.DOMDocument.3.0", "MSXML2.DOMDocument",
            "Microsoft.XmlDom"];
    
            for (var i=0; i < arrSignatures.length; i++)
            {
                try {
                    xmlDom = new ActiveXObject(arrSignatures[i]);
                    xmlDom.async = false;
                    return xmlDom;
                } catch (oError) {
                    this.xmlDom = false;
                }
            }         
            throw new Error("MSXML is not installed on your system.");        
        }
        else if (document.implementation && document.implementation.createDocument) //FireFox,NetScape,ect...
        {
            if(document.implementation.hasFeature("XPath", "3.0") )
            {
                // prototying the XMLDocument
                XMLDocument.prototype.selectNodes = function(cXPathString, xNode)
                {
                    if( !xNode ) { xNode = this; } 
                    var oNSResolver = this.createNSResolver(this.documentElement)
                    var aItems = this.evaluate(cXPathString, xNode, oNSResolver, 
                    XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null)
                    var aResult = [];
                    for( var i = 0; i < aItems.snapshotLength; i++)
                    {
                        aResult[i] =    aItems.snapshotItem(i);
                    }
                    return aResult;
                }
               
                // prototying the Element
                Element.prototype.selectNodes = function(cXPathString)
                {
                    if(this.ownerDocument.selectNodes) {
                        return this.ownerDocument.selectNodes(cXPathString, this);
                    } else {
                        throw "For XML Elements Only";
                    }
                }
                   
                // prototying the XMLDocument
                XMLDocument.prototype.selectSingleNode = function(cXPathString, xNode)
                {
                    if( !xNode ) { xNode = this; } 
                    var xItems = this.selectNodes(cXPathString, xNode);
                    if( xItems.length > 0 )
                    {
                        return xItems[0];
                    }
                    else
                    {
                        return null;
                    }
                }
    
                // prototying the Element
                Element.prototype.selectSingleNode = function(cXPathString)
                {    
                    if(this.ownerDocument.selectSingleNode)
                    {
                        return this.ownerDocument.selectSingleNode(cXPathString, this);
                    } else {
                        throw "For XML Elements Only";
                    }
                }
    
                XMLDocument.prototype.loadXML = function(xmlString)
                {
                    var childNodes = this.childNodes;
                    for (var i = childNodes.length - 1; i >= 0; i--)
                        this.removeChild(childNodes[i]);
    
                    var dp = new DOMParser();
                    var newDOM = dp.parseFromString(xmlString, "text/xml");
                    var newElem = this.importNode(newDOM.documentElement, true);
                    this.appendChild(newElem);
                }
            }
           
            xmlDom = document.implementation.createDocument("","",null);
            xmlDom.async = false;
            return xmlDom;
        } 
        else 
        {
            throw new Error("Your browser doesn't support an XML DOM object.");
        }
    }
}
