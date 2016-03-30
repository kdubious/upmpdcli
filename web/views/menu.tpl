<div id="menu">
  <ul id="menu">
%if current == 'list':
    <li>List Receivers</li>
%else:
    <li><a href="list">List Receivers</a></li>
%end
%if current == 'assoc':
    <li>Associate Receivers</li>
%else:
    <li><a href="assoc">Associate Receivers</a></li>
%end
%if current == 'stop':
    <li>Stop Receivers</li>
%else:
    <li><a href="stop">Stop Receivers</a></li>
%end
  </ul>
</div>

